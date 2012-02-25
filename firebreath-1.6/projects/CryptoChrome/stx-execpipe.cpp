// -*- mode: c++; fill-column: 79 -*-
// $Id: stx-execpipe.cc 16 2010-07-30 15:04:11Z tb $

/*
 * STX Execution Pipe Library v0.7.1
 * Copyright (C) 2010 Timo Bingmann
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// this should be set by autoconf. check it again here if compiled separately.
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "stx-execpipe.h"

#include <stdexcept>
#include <sstream>
#include <iostream>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>

#define LOG_OUTPUT(msg, level)                           \
    do {                                                 \
        if (m_debug_level >= level) {                    \
            std::ostringstream oss;                      \
            oss << msg;                                  \
            if (m_debug_output)                          \
                m_debug_output(oss.str().c_str());       \
            else                                         \
                std::cout << oss.str() << std::endl;     \
        }                                                \
    } while (0)

#define LOG_ERROR(msg)	LOG_OUTPUT(msg, ExecPipe::DL_ERROR)
#define LOG_INFO(msg)	LOG_OUTPUT(msg, ExecPipe::DL_INFO)
#define LOG_DEBUG(msg)	LOG_OUTPUT(msg, ExecPipe::DL_DEBUG)
#define LOG_TRACE(msg)	LOG_OUTPUT(msg, ExecPipe::DL_TRACE)

namespace stx {

#ifndef _STX_RINGBUFFER_H_
#define _STX_RINGBUFFER_H_

/// namespace containing RingBuffer utility class
namespace {

/**
 * RingBuffer is a byte-oriented, pipe memory buffer which uses the underlying
 * space in a circular fashion.
 * 
 * The input stream is write()en into the buffer as blocks of bytes, while the
 * buffer is reallocated with exponential growth as needed.
 *
 * The first unread byte can be accessed using bottom(). The number of unread
 * bytes at the ring buffers bottom position is queried by bottomsize(). This
 * may not match the total number of unread bytes as returned by size(). After
 * processing the bytes at bottom(), the unread cursor may be moved using
 * advance().
 *
 * The ring buffer has the following two states.
 * <pre>
 * +------------------------------------------------------------------+
 * | unused     |                 data   |               unused       |
 * +------------+------------------------+----------------------------+
 *              ^                        ^
 *              m_bottom                 m_bottom+m_size
 * </pre>
 *
 * or 
 *
 * <pre>
 * +------------------------------------------------------------------+
 * | more data  |                 unused               | data         |
 * +------------+--------------------------------------+--------------+
 *              ^                                      ^
 *              m_bottom+m_size                        m_bottom
 * </pre>
 *
 * The size of the whole buffer is m_buffsize.
 */
class RingBuffer
{
private:
    /// pointer to allocated memory buffer
    char*		m_data;

    /// number of bytes allocated in m_data
    unsigned int	m_buffsize;

    /// number of unread bytes in ring buffer
    unsigned int	m_size;

    /// bottom pointer of unread area
    unsigned int 	m_bottom;

public:
    /// Construct an empty ring buffer.
    inline RingBuffer()
	: m_data(NULL),
	  m_buffsize(0), m_size(0), m_bottom(0)
    {
    }

    /// Free the possibly used memory space.
    inline ~RingBuffer()
    {
	if (m_data) free(m_data);
    }
    
    /// Return the current number of unread bytes.
    inline unsigned int size() const
    {
	return m_size;
    }

    /// Return the current number of allocated bytes.
    inline unsigned int buffsize() const
    {
	return m_buffsize;
    }

    /// Reset the ring buffer to empty.
    inline void clear()
    {
	m_size = m_bottom = 0;
    }

    /**
     * Return a pointer to the first unread element. Be warned that the buffer
     * may not be linear, thus bottom()+size() might not be valid. You have to
     * use bottomsize().
     */
    inline char* bottom() const
    {
	return m_data + m_bottom;
    }

    /// Return the number of bytes available at the bottom() place.
    inline unsigned int bottomsize() const
    {
	return (m_bottom + m_size > m_buffsize)
	    ? (m_buffsize - m_bottom)
	    : (m_size);
    }

    /**
     * Advance the internal read pointer n bytes, thus marking that amount of
     * data as read.
     */
    inline void advance(unsigned int n)
    {
	assert(m_size >= n);
	m_bottom += n;
	m_size -= n;
	if (m_bottom >= m_buffsize) m_bottom -= m_buffsize;
    }

    /**
     * Write len bytes into the ring buffer at the top position, the buffer
     * will grow if necessary.
     */
    void write(const void *src, unsigned int len)
    {
	if (len == 0) return;

	if (m_buffsize < m_size + len)
	{
	    // won't fit, we have to grow the buffer, we'll grow the buffer to
	    // twice the size.

	    unsigned int newbuffsize = m_buffsize;
	    while (newbuffsize < m_size + len)
	    {
		if (newbuffsize == 0) newbuffsize = 1024;
		else newbuffsize = newbuffsize * 2;
	    }

	    m_data = static_cast<char*>(realloc(m_data, newbuffsize));

	    if (m_bottom + m_size > m_buffsize)
	    {
		// copy the ringbuffer's tail to the new buffer end, use memcpy
		// here because there cannot be any overlapping area.

		unsigned int taillen = m_buffsize - m_bottom;

		memcpy(m_data + newbuffsize - taillen,
		       m_data + m_bottom, taillen);

		m_bottom = newbuffsize - taillen;
	    }

	    m_buffsize = newbuffsize;
	}

	// block now fits into the buffer somehow

	// check if the new memory fits into the middle space
	if (m_bottom + m_size > m_buffsize)
	{
	    memcpy(m_data + m_bottom + m_size - m_buffsize, src, len);
	    m_size += len;
	}
	else 
	{
	    // first fill up the buffer's tail, which has tailfit bytes room
	    unsigned int tailfit = m_buffsize - (m_bottom + m_size);

	    if (tailfit >= len)
	    {
		memcpy(m_data + m_bottom + m_size, src, len);
		m_size += len;
	    }
	    else
	    {
		// doesn't fit into the tail alone, we have to break it up
		memcpy(m_data + m_bottom + m_size, src, tailfit);
		memcpy(m_data, reinterpret_cast<const char*>(src) + tailfit,
		       len - tailfit);
		m_size += len;
	    }
	}
    }
};

} // namespace <anonymous>

#endif // _STX_RINGBUFFER_H_

/**
 * \brief Main library implementation (internal object)
 *
 * Implementation class for stx::ExecPipe. See the documentation of the
 * front-end class for detailed information.
 */
class ExecPipeImpl
{
private:

    /// reference counter
    unsigned int	m_refs;

private:

    // *** Debugging Output ***
    
    /// currently set debug level
    enum ExecPipe::DebugLevel	m_debug_level;

    /// current debug line output function
    void		(*m_debug_output)(const char* line);

public:

    /// Change the current debug level. The default is DL_ERROR.
    void set_debug_level(enum ExecPipe::DebugLevel dl)
    {
	m_debug_level = dl;
    }

    /// Change output function for debug messages. If set to NULL (the default)
    /// the debug lines are printed to stdout.
    void set_debug_output(void (*output)(const char *line))
    {
	m_debug_output = output;
    }

private:

    /// Enumeration describing the currently set input or output stream type
    enum StreamType
    {
	ST_NONE = 0,	///< no special redirection requested
	ST_FD,		///< redirection to existing fd
	ST_FILE,	///< redirection to file path
	ST_STRING,	///< input/output directed by/to string
	ST_OBJECT	///< input/output attached to program object
    };

    /// describes the currently set input stream type
    StreamType		m_input;

    // *** Input Stream ***

    /// for ST_FD the input fd given by the user. for ST_STRING and ST_FUNCTION
    /// the pipe write fd of the parent process.
    int			m_input_fd;

    /// for ST_FILE the path of the input file.
    const char* 	m_input_file;

    /// for ST_STRING a pointer to the user-supplied std::string input stream
    /// object.
    const std::string*	m_input_string;

    /// for ST_STRING the current position in the input stream object.
    std::string::size_type m_input_string_pos;

    /// for ST_OBJECT the input stream source object
    PipeSource*		m_input_source;

    /// for ST_OBJECT the input stream ring buffer
    RingBuffer		m_input_rbuffer;

    // *** Output Stream ***

    /// describes the currently set input stream type
    StreamType		m_output;

    /// for ST_FD the output fd given by the user. for ST_STRING and
    /// ST_FUNCTION the pipe read fd of the parent process.
    int			m_output_fd;

    /// for ST_FILE the path of the output file.
    const char*		m_output_file;

    /// for ST_FILE the permission used in the open() call.
    int			m_output_file_mode;

    /// for ST_STRING a pointer to the user-supplied std::string output stream
    /// object.
    std::string*	m_output_string;

    /// for ST_OBJECT the output stream source object
    PipeSink*		m_output_sink;

    // *** Pipe Stages ***

    /**
     * Structure representing each stage in the pipe. Contains arguments,
     * buffers and output variables.
     */
    struct Stage
    {
	/// List of program and arguments copied from simple add_exec() calls.
	std::vector<std::string>	args;

	/// Character pointer to program path called
	const char*			prog;

	/// Pointer to user list of program and arguments.
	const std::vector<std::string>* argsp;

	/// Pointer to environment list supplied by user.
	const std::vector<std::string>* envp;

	/// Pipe stage function object.
	PipeFunction*			func;

	/// Output stream buffer for function object.
	RingBuffer			outbuffer;

	// *** Exec Stages Variables ***

	/// Call execp() variants.
	bool	withpath;

	/// Pid of the running child process
	pid_t	pid;

	/// Return status of wait() after child exit.
	int	retstatus;
	
	/// File descriptor for child stdin. This is dup2()-ed to STDIN.
	int	stdin_fd;

	/// File descriptor for child stdout. This is dup2()-ed to STDOUT.
	int	stdout_fd;

	/// Constructor reseting all variables.
	Stage()
	    : prog(NULL), argsp(NULL), envp(NULL), func(NULL),
	      withpath(false), pid(0), retstatus(0),
	      stdin_fd(-1), stdout_fd(-1)
	{
	}
    };

    /// typedef of list of pipe stages.
    typedef std::vector<Stage> stagelist_type;

    /// list of pipe stages.
    stagelist_type	m_stages;

    /// general buffer used for read() and write() calls.
    char		m_buffer[4096];

public:

    /// Create a new pipe implementation with zero reference counter.
    ExecPipeImpl()
	: m_refs(0),
	  m_debug_level(ExecPipe::DL_ERROR),
	  m_debug_output(NULL),
	  m_input(ST_NONE),
	  m_input_fd(-1),
	  m_output(ST_NONE),
	  m_output_fd(-1)
    {
    }

    /// Return writable reference to counter.
    unsigned int& refs()
    {
	return m_refs;
    }

    // *** Input Selectors ***

    ///@{ \name Input Selectors

    /**
     * Assign an already opened file descriptor as input stream for the first
     * exec stage.
     */
    void set_input_fd(int fd)
    {
	assert(m_input == ST_NONE);
	if (m_input != ST_NONE) return;

	m_input = ST_FD;
	m_input_fd = fd;
    }

    /**
     * Assign a file as input stream source. This file will be opened read-only
     * and read by the first exec stage.
     */
    void set_input_file(const char* path)
    {
	assert(m_input == ST_NONE);
	if (m_input != ST_NONE) return;

	m_input = ST_FILE;
	m_input_file = path;
    }    

    /**
     * Assign a std::string as input stream source. The contents of the string
     * will be written to the first exec stage. The string object is not copied
     * and must still exist when run() is called.
     */
    void set_input_string(const std::string* input)
    {
	assert(m_input == ST_NONE);
	if (m_input != ST_NONE) return;

	m_input = ST_STRING;
	m_input_string = input;
	m_input_string_pos = 0;
    }

    /**
     * Assign a PipeSource as input stream source. The object will be queried
     * via the read() function for data which is then written to the first exec
     * stage.
     */
    void set_input_source(PipeSource* source)
    {
	assert(m_input == ST_NONE);
	if (m_input != ST_NONE) return;

	m_input = ST_OBJECT;
	m_input_source = source;
	source->m_impl = this;
    }

    ///@}
    
    /**
     * Function called by PipeSource::write() to push data into the ring
     * buffer.
     */
    void input_source_write(const void* data, unsigned int datalen)
    {
	m_input_rbuffer.write(data, datalen);
    }

    // *** Output Selectors ***

    ///@{ \name Output Selectors

    /**
     * Assign an already opened file descriptor as output stream for the last
     * exec stage.
     */
    void set_output_fd(int fd)
    {
	assert(m_output == ST_NONE);
	if (m_output != ST_NONE) return;

	m_output = ST_FD;
	m_output_fd = fd;
    }

    /**
     * Assign a file as output stream destination. This file will be created or
     * truncated write-only and written by the last exec stage.
     */
    void set_output_file(const char* path, int mode = 0666)
    {
	assert(m_output == ST_NONE);
	if (m_output != ST_NONE) return;

	m_output = ST_FILE;
	m_output_file = path;
	m_output_file_mode = mode;
    }    

    /**
     * Assign a std::string as output stream destination. The output of the
     * last exec stage will be stored as the contents of the string. The string
     * object is not copied and must still exist when run() is called.
     */
    void set_output_string(std::string* output)
    {
	assert(m_output == ST_NONE);
	if (m_output != ST_NONE) return;

	m_output = ST_STRING;
	m_output_string = output;
    }

    /**
     * Assign a PipeSink as output stream destination. The object will receive
     * data via the process() function and is informed via eof()
     */
    void set_output_sink(PipeSink* sink)
    {
	assert(m_output == ST_NONE);
	if (m_output != ST_NONE) return;

	m_output = ST_OBJECT;
	m_output_sink = sink;
    }

    ///@}

    // *** Pipe Stages ***

    ///@{ \name Add Pipe Stages

    /**
     * Return the number of pipe stages added.
     */
    unsigned int size() const
    {
	return m_stages.size();
    }

    /**
     * Add an exec() stage to the pipe with given arguments. Note that argv[0]
     * is set to prog.
     */
    void add_exec(const char* prog)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	m_stages.push_back(newstage);
    }

    /**
     * Add an exec() stage to the pipe with given arguments. Note that argv[0]
     * is set to prog.
     */
    void add_exec(const char* prog, const char* arg1)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.args.push_back(arg1);
	m_stages.push_back(newstage);
    }

    /**
     * Add an exec() stage to the pipe with given arguments. Note that argv[0]
     * is set to prog.
     */
    void add_exec(const char* prog, const char* arg1, const char* arg2)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.args.push_back(arg1);
	newstage.args.push_back(arg2);
	m_stages.push_back(newstage);
    }

    /**
     * Add an exec() stage to the pipe with given arguments. Note that argv[0]
     * is set to prog.
     */
    void add_exec(const char* prog, const char* arg1, const char* arg2, const char* arg3)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.args.push_back(arg1);
	newstage.args.push_back(arg2);
	newstage.args.push_back(arg3);
	m_stages.push_back(newstage);
    }

    /**
     * Add an exec() stage to the pipe with given arguments. The vector of
     * arguments is not copied, so it must still exist when run() is
     * called. Note that the program called is args[0].
     */
    void add_exec(const std::vector<std::string>* args)
    {
	assert(args->size() > 0);
	if (args->size() == 0) return;

	struct Stage newstage;
	newstage.prog = (*args)[0].c_str();
	newstage.argsp = args;
	m_stages.push_back(newstage);
    }

    /**
     * Add an execp() stage to the pipe with given arguments. The PATH variable
     * is search for programs not containing a slash / character. Note that
     * argv[0] is set to prog.
     */
    void add_execp(const char* prog)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.withpath = true;
	m_stages.push_back(newstage);
    }

    /**
     * Add an execp() stage to the pipe with given arguments. The PATH variable
     * is search for programs not containing a slash / character. Note that
     * argv[0] is set to prog.
     */
    void add_execp(const char* prog, const char* arg1)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.args.push_back(arg1);
	newstage.withpath = true;
	m_stages.push_back(newstage);
    }

    /**
     * Add an execp() stage to the pipe with given arguments. The PATH variable
     * is search for programs not containing a slash / character. Note that
     * argv[0] is set to prog.
     */
    void add_execp(const char* prog, const char* arg1, const char* arg2)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.args.push_back(arg1);
	newstage.args.push_back(arg2);
	newstage.withpath = true;
	m_stages.push_back(newstage);
    }

    /**
     * Add an execp() stage to the pipe with given arguments. The PATH variable
     * is search for programs not containing a slash / character. Note that
     * argv[0] is set to prog.
     */
    void add_execp(const char* prog, const char* arg1, const char* arg2, const char* arg3)
    {
	struct Stage newstage;
	newstage.prog = prog;
	newstage.args.push_back(prog);
	newstage.args.push_back(arg1);
	newstage.args.push_back(arg2);
	newstage.args.push_back(arg3);
	newstage.withpath = true;
	m_stages.push_back(newstage);
    }

    /**
     * Add an execp() stage to the pipe with given arguments. The PATH variable
     * is search for programs not containing a slash / character. The vector of
     * arguments is not copied, so it must still exist when run() is
     * called. Note that the program called is args[0].
     */
    void add_execp(const std::vector<std::string>* args)
    {
	assert(args->size() > 0);
	if (args->size() == 0) return;

	struct Stage newstage;
	newstage.prog = (*args)[0].c_str();
	newstage.argsp = args;
	newstage.withpath = true;
	m_stages.push_back(newstage);
    }

    /**
     * Add an exece() stage to the pipe with the given arguments and
     * environments. This is the most flexible exec() call. The vector of
     * arguments and environment variables is not copied, so it must still
     * exist when run() is called. The env vector pointer may be NULL, the args
     * vector must not be NULL. The args[0] is _not_ override with path, so you
     * can fake program name calls.
     */
    void add_exece(const char* path,
		   const std::vector<std::string>* argsp,
		   const std::vector<std::string>* envp)
    {
	assert(path && argsp);
	assert(argsp->size() > 0);
	if (argsp->size() == 0) return;

	struct Stage newstage;
	newstage.prog = path;
	newstage.argsp = argsp;
	newstage.envp = envp;
	m_stages.push_back(newstage);
    }

    /**
     * Add a function stage to the pipe. This function object will be called in
     * the parent process with data passing through the stage. See PipeFunction
     * for more information.
     */
    void add_function(PipeFunction* func)
    {
	assert(func);
	if (!func) return;

	func->m_impl = this;
	func->m_stageid = m_stages.size();

	struct Stage newstage;
	newstage.func = func;
	m_stages.push_back(newstage);
    }

    ///@}

    /**
     * Function called by PipeSource::write() to push data into the ring
     * buffer.
     */
    void stage_function_write(unsigned int st, const void* data, unsigned int datalen)
    {
	assert(st < m_stages.size());

	return m_stages[st].outbuffer.write(data, datalen);
    }

    // *** Run Pipe ***

    /**
     * Run the configured pipe sequence and wait for all children processes to
     * complete. Returns a reference to *this for chaining.
     *
     * This function call should be wrapped into a try-catch block as it will
     * throw() if a system call fails.
     */
    void run();

    // *** Inspection After Pipe Execution ***

    ///@{ \name Inspect Return Codes

    /**
     * Get the return status of exec() stage's program run after pipe execution
     * as indicated by wait().
     */
    int get_return_status(unsigned int stageid) const
    {
	assert(stageid < m_stages.size());
	assert(!m_stages[stageid].func);

	return m_stages[stageid].retstatus;
    }

    /**
     * Get the return code of exec() stage's program run after pipe execution,
     * or -1 if the program terminated abnormally.
     */
    int get_return_code(unsigned int stageid) const
    {
	assert(stageid < m_stages.size());
	assert(!m_stages[stageid].func);

	if (WIFEXITED(m_stages[stageid].retstatus))
	    return WEXITSTATUS(m_stages[stageid].retstatus);
	else
	    return -1;
    }

    /**
     * Get the signal of the abnormally terminated exec() stage's program run
     * after pipe execution, or -1 if the program terminated normally.
     */
    int get_return_signal(unsigned int stageid) const
    {
	assert(stageid < m_stages.size());
	assert(!m_stages[stageid].func);

	if (WIFSIGNALED(m_stages[stageid].retstatus))
	    return WTERMSIG(m_stages[stageid].retstatus);
	else
	    return -1;
    }

    /**
     * Return true if the return code of all exec() stages were zero.
     */
    bool all_return_codes_zero() const
    {
	for (unsigned int i = 0; i < m_stages.size(); ++i)
	{
	    if (m_stages[i].func) continue;

	    if (get_return_code(i) != 0)
		return false;
	}

	return true;
    }

    ///@}

protected:

    // *** Helper Function for run() ***

    /// Transform arguments and launch an exec stage using the correct exec()
    /// variant.
    void	exec_stage(const Stage& stage);

    /// Print all arguments of exec() call.
    void	print_exec(const std::vector<std::string>& args);

    /// Safe close() call and output error if fd was already closed.
    void	sclose(int fd);
};

// --- ExecPipeImpl ----------------------------------------------------- //

void ExecPipeImpl::print_exec(const std::vector<std::string>& args)
{
    std::ostringstream oss;
    oss << "Exec()";
    for (unsigned ai = 0; ai < args.size(); ++ai)
    {
	oss << " " << args[ai];
    }
    LOG_INFO(oss.str());
}

void ExecPipeImpl::exec_stage(const Stage& stage)
{
    // select arguments vector
    const std::vector<std::string>& args = stage.argsp ? *stage.argsp : stage.args;

    // create const char*[] of prog and arguments for syscall.

    const char* cargs[args.size()+1];

    for (unsigned ai = 0; ai < args.size(); ++ai)
    {
	cargs[ai] = args[ai].c_str();
    }
    cargs[ args.size() ] = NULL;

    if (!stage.envp)
    {
	if (stage.withpath)
	    execvp(stage.prog, (char* const*)cargs);
	else
	    execv(stage.prog, (char* const*)cargs);
    }
    else
    {
	// create envp const char*[] for syscall.

	const char* cenv[args.size()+1];

	for (unsigned ei = 0; ei < stage.envp->size(); ++ei)
	{	
	    cenv[ei] = (*stage.envp)[ei].c_str();
	}
	cenv[ stage.envp->size() ] = NULL;

	execve(stage.prog, (char* const*)cargs, (char* const*)cenv);
    }

    LOG_ERROR("Error executing child process: " << strerror(errno));
}

void ExecPipeImpl::sclose(int fd)
{
    int r = close(fd);

    if (r != 0) {
	LOG_ERROR("Could not correctly close fd: " << strerror(errno));
    }
}

// --- ExecPipeImpl::run() ---------------------------------------------- //

void ExecPipeImpl::run()
{
    if (m_stages.size() == 0)
	throw(std::runtime_error("No stages to in exec pipe."));

    // *** Phase 1: prepare all file descriptors ************************* //

    // set up input stream accordingly
    switch(m_input)
    {
    case ST_NONE:
	// no file change of file descriptor after fork.
	m_stages[0].stdin_fd = -1;
	break;

    case ST_STRING:
    case ST_OBJECT: {
	// create input pipe for strings and function objects.
	int pipefd[2];

	if (pipe(pipefd) != 0)
	    throw(std::runtime_error(std::string("Could not create an input pipe: ") + strerror(errno)));

	if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) != 0)
	    throw(std::runtime_error(std::string("Could not set non-block mode on input pipe: ") + strerror(errno)));

	m_input_fd = pipefd[1];
	m_stages[0].stdin_fd = pipefd[0];
	break;
    }
    case ST_FILE: {
	// open input file

	int infd = open(m_input_file, O_RDONLY);
	if (infd < 0)
	    throw(std::runtime_error(std::string("Could not open input file: ") + strerror(errno)));

	m_stages[0].stdin_fd = infd;
	break;
    }
    case ST_FD:
	// assign user-provided fd to first process
	m_stages[0].stdin_fd = m_input_fd;
	m_input_fd = -1;
	break;
    }

    // create pipes between exec stages
    for (unsigned int i = 0; i < m_stages.size() - 1; ++i)
    {
	int pipefd[2];

	if (pipe(pipefd) != 0)
	    throw(std::runtime_error(std::string("Could not create a stage pipe: ") + strerror(errno)));

	m_stages[i].stdout_fd = pipefd[1];
	m_stages[i+1].stdin_fd = pipefd[0];

	if (m_stages[i].func)
	{
	    if (fcntl(m_stages[i].stdout_fd, F_SETFL, O_NONBLOCK) != 0)
		throw(std::runtime_error(std::string("Could not set non-block mode on a stage pipe: ") + strerror(errno)));
	}
	if (m_stages[i+1].func)
	{
	    if (fcntl(m_stages[i+1].stdin_fd, F_SETFL, O_NONBLOCK) != 0)
		throw(std::runtime_error(std::string("Could not set non-block mode on a stage pipe: ") + strerror(errno)));
	}
    }

    // set up output stream accordingly
    switch(m_output)
    {
    case ST_NONE:
	// no file change of file descriptor after fork.
	m_stages.back().stdout_fd = -1;
	break;

    case ST_STRING: 
    case ST_OBJECT: {
	// create output pipe for strings and objects.
	int pipefd[2];

	if (pipe(pipefd) != 0)
	    throw(std::runtime_error(std::string("Could not create an output pipe: ") + strerror(errno)));

	if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) != 0)
	    throw(std::runtime_error(std::string("Could not set non-block mode on output pipe: ") + strerror(errno)));

	m_stages.back().stdout_fd = pipefd[1];
	m_output_fd = pipefd[0];
	break;
    }
    case ST_FILE: {
	// create or truncate output file

	int outfd = open(m_output_file, O_WRONLY | O_CREAT | O_TRUNC, m_output_file_mode);
	if (outfd < 0)
	    throw(std::runtime_error(std::string("Could not open output file: ") + strerror(errno)));

	m_stages.back().stdout_fd = outfd;
	break;
    }
    case ST_FD:
	// assign user-provided fd to last process
	m_stages.back().stdout_fd = m_output_fd;
	m_output_fd = -1;
	break;
    }

    // *** Phase 2: launch child processes ******************************* //

    for (unsigned int i = 0; i < m_stages.size(); ++i)
    {
	if (m_stages[i].func) continue;

	print_exec(m_stages[i].args);

	pid_t child = fork();
	if (child == 0)
	{
	    // inside child process

	    // move assigned file descriptors and close all others
	    if (m_input_fd >= 0)
		sclose(m_input_fd);

	    for (unsigned int j = 0; j < m_stages.size(); ++j)
	    {
		if (i == j)
		{
		    // dup2 file descriptors assigned for this stage as stdin and stdout

		    if (m_stages[i].stdin_fd >= 0)
		    {
			if (dup2(m_stages[i].stdin_fd, STDIN_FILENO) == -1) {
			    LOG_ERROR("Could not redirect file descriptor: " << strerror(errno));
			    exit(255);
			}
		    }

		    if (m_stages[i].stdout_fd >= 0)
		    {
			if (dup2(m_stages[i].stdout_fd, STDOUT_FILENO) == -1) {
			    LOG_ERROR("Could not redirect file descriptor: " << strerror(errno));
			    exit(255);
			}
		    }
		}
		else
		{
		    // close file descriptors of other stages

		    if (m_stages[j].stdin_fd >= 0)
			sclose(m_stages[j].stdin_fd);

		    if (m_stages[j].stdout_fd >= 0)
			sclose(m_stages[j].stdout_fd);
		}
	    }

	    if (m_output_fd >= 0)
		sclose(m_output_fd);

	    // run program
	    exec_stage(m_stages[i]);

	    exit(255);
	}

	m_stages[i].pid = child;
    }

    // parent process: close all unneeded file descriptors of exec stages.

    for (stagelist_type::const_iterator st = m_stages.begin();
	 st != m_stages.end(); ++st)
    {
	if (st->func) continue;

	if (st->stdin_fd >= 0)
	    sclose(st->stdin_fd);

	if (st->stdout_fd >= 0)
	    sclose(st->stdout_fd);
    }

    // *** Phase 3: run select() loop and process data ******************* //

    while(1)
    {
	// build file descriptor sets

	int max_fds = -1;
	fd_set read_fds, write_fds;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	if (m_input_fd >= 0)
	{
	    if (m_input == ST_OBJECT)
	    {
		assert(m_input_source);

		if (!m_input_rbuffer.size() && !m_input_source->poll() && !m_input_rbuffer.size())
		{
		    sclose(m_input_fd);
		    m_input_fd = -1;

		    LOG_INFO("Closing input file descriptor: " << strerror(errno));
		}
		else
		{
		    FD_SET(m_input_fd, &write_fds);
		    if (max_fds < m_input_fd) max_fds = m_input_fd;

		    LOG_DEBUG("Select on input file descriptor");
		}
	    }
	    else
	    {
		FD_SET(m_input_fd, &write_fds);
		if (max_fds < m_input_fd) max_fds = m_input_fd;

		LOG_DEBUG("Select on input file descriptor");
	    }
	}

	for (unsigned int i = 0; i < m_stages.size(); ++i)
	{
	    if (!m_stages[i].func) continue;

	    if (m_stages[i].stdin_fd >= 0)
	    {
		FD_SET(m_stages[i].stdin_fd, &read_fds);
		if (max_fds < m_stages[i].stdin_fd) max_fds = m_stages[i].stdin_fd;

		LOG_DEBUG("Select on stage input file descriptor");
	    }

	    if (m_stages[i].stdout_fd >= 0)
	    {
		if (m_stages[i].outbuffer.size())
		{
		    FD_SET(m_stages[i].stdout_fd, &write_fds);
		    if (max_fds < m_stages[i].stdout_fd) max_fds = m_stages[i].stdout_fd;

		    LOG_DEBUG("Select on stage output file descriptor");
		}
		else if (m_stages[i].stdin_fd < 0 && !m_stages[i].outbuffer.size())
		{
		    sclose(m_stages[i].stdout_fd);
		    m_stages[i].stdout_fd = -1;

		    LOG_INFO("Close stage output file descriptor");
		}
	    }
	}

	if (m_output_fd >= 0)
	{
	    FD_SET(m_output_fd, &read_fds);
	    if (max_fds < m_output_fd) max_fds = m_output_fd;

	    LOG_DEBUG("Select on output file descriptor");
	}

	// issue select() call

	if (max_fds < 0)
	    break;

	int retval = select(max_fds+1, &read_fds, &write_fds, NULL, NULL);
	if (retval < 0)
	    throw(std::runtime_error(std::string("Error during select() on file descriptors: ") + strerror(errno)));

	LOG_TRACE("select() on " << retval << " file descriptors: " << strerror(errno));

	// handle file descriptors marked by select() in both sets

	if (m_input_fd >= 0 && FD_ISSET(m_input_fd, &write_fds))
	{
	    if (m_input == ST_STRING)
	    {
		// write string data to first stdin file descriptor.

		assert(m_input_string);
		assert(m_input_string_pos < m_input_string->size());

		ssize_t wb;

		do
		{
		    wb = write(m_input_fd,
			       m_input_string->data() + m_input_string_pos,
			       m_input_string->size() - m_input_string_pos);

		    LOG_TRACE("Write on input fd: " << wb);

		    if (wb < 0)
		    {
			if (errno == EAGAIN || errno == EINTR)
			{
			}
			else
			{
			    LOG_DEBUG("Error writing to input file descriptor: " << strerror(errno));

			    sclose(m_input_fd);
			    m_input_fd = -1;

			    LOG_INFO("Closing input file descriptor: " << strerror(errno));
			}
		    }
		    else if (wb > 0)
		    {
			m_input_string_pos += wb;

			if (m_input_string_pos >= m_input_string->size())
			{
			    sclose(m_input_fd);
			    m_input_fd = -1;

			    LOG_INFO("Closing input file descriptor: " << strerror(errno));
			    break;
			}
		    }
		} while (wb > 0);

	    }
	    else if (m_input == ST_OBJECT)
	    {
		// write buffered data to first stdin file descriptor.
		
		ssize_t wb;

		do
		{
		    wb = write(m_input_fd,
			       m_input_rbuffer.bottom(),
			       m_input_rbuffer.bottomsize());

		    LOG_TRACE("Write on input fd: " << wb);

		    if (wb < 0)
		    {
			if (errno == EAGAIN || errno == EINTR)
			{
			}
			else
			{
			    LOG_INFO("Error writing to input file descriptor: " << strerror(errno));

			    sclose(m_input_fd);
			    m_input_fd = -1;

			    LOG_INFO("Closing input file descriptor: " << strerror(errno));
			}
		    }
		    else if (wb > 0)
		    {
			m_input_rbuffer.advance(wb);
		    }
		} while (wb > 0);
	    }
	}

	if (m_output_fd >= 0 && FD_ISSET(m_output_fd, &read_fds))
	{
	    // read data from last stdout file descriptor

	    ssize_t rb;

	    do
	    {
		errno = 0;

		rb = read(m_output_fd, 
			  m_buffer, sizeof(m_buffer));

		LOG_TRACE("Read on output fd: " << rb);

		if (rb <= 0)
		{
		    if (rb == 0 && errno == 0)
		    {
			// zero read indicates eof

			LOG_INFO("Closing output file descriptor: " << strerror(errno));

			if (m_output == ST_OBJECT)
			{
			    assert(m_output_sink);
			    m_output_sink->eof();
			}

			sclose(m_output_fd);
			m_output_fd = -1;
		    }
		    else if (errno == EAGAIN || errno == EINTR)
		    {
		    }
		    else
		    {
			LOG_ERROR("Error reading from output file descriptor: " << strerror(errno));
		    }
		}
		else
		{
		    if (m_output == ST_STRING)
		    {
			assert(m_output_string);
			m_output_string->append(m_buffer, rb);
		    }
		    else if (m_output == ST_OBJECT)
		    {
			assert(m_output_sink);
			m_output_sink->process(m_buffer, rb);
		    }
		}
	    } while (rb > 0);
	}
	    
	for (unsigned int i = 0; i < m_stages.size(); ++i)
	{
	    if (!m_stages[i].func) continue;

	    if (m_stages[i].stdin_fd >= 0 && FD_ISSET(m_stages[i].stdin_fd, &read_fds))
	    {
		ssize_t rb;

		do
		{
		    errno = 0;

		    rb = read(m_stages[i].stdin_fd, 
			      m_buffer, sizeof(m_buffer));

		    LOG_TRACE("Read on stage fd: " << rb);

		    if (rb <= 0)
		    {
			if (rb == 0 && errno == 0)
			{
			    // zero read indicates eof

			    LOG_INFO("Closing stage input file descriptor: " << strerror(errno));

			    m_stages[i].func->eof();

			    sclose(m_stages[i].stdin_fd);
			    m_stages[i].stdin_fd = -1;
			}
			else if (errno == EAGAIN || errno == EINTR)
			{
			}
			else
			{
			    LOG_ERROR("Error reading from stage input file descriptor: " << strerror(errno));
			}
		    }
		    else
		    {
			m_stages[i].func->process(m_buffer, rb);
		    }
		} while (rb > 0);
	    }

	    if (m_stages[i].stdout_fd >= 0 && FD_ISSET(m_stages[i].stdout_fd, &write_fds))
	    {
		while (m_stages[i].outbuffer.size() > 0)
		{
		    ssize_t wb = write(m_stages[i].stdout_fd,
				       m_stages[i].outbuffer.bottom(),
				       m_stages[i].outbuffer.bottomsize());

		    LOG_TRACE("Write on stage fd: " << wb);

		    if (wb < 0)
		    {
			if (errno == EAGAIN || errno == EINTR)
			{
			}
			else
			{
			    LOG_INFO("Error writing to stage output file descriptor: " << strerror(errno));
			}
			break;
		    }
		    else if (wb > 0)
		    {
			m_stages[i].outbuffer.advance(wb);
		    }
		}

		if (m_stages[i].stdin_fd < 0 && !m_stages[i].outbuffer.size())
		{
		    LOG_INFO("Closing stage output file descriptor: " << strerror(errno));

		    sclose(m_stages[i].stdout_fd);
		    m_stages[i].stdout_fd = -1;
		}
	    }
	}
    }

    // *** Phase 4: call wait() for all children processes *************** //

    unsigned int donepid = 0;

    for (unsigned int i = 0; i < m_stages.size(); ++i)
    {
	if (!m_stages[i].func) continue;
	++donepid;
    }

    while (donepid != m_stages.size())
    {
	int status;
	int p = wait(&status);

	if (p < 0)
	{
	    LOG_ERROR("Error calling wait(): " << strerror(errno));
	    break;
	}

	bool found = false;

	for (unsigned int i = 0; i < m_stages.size(); ++i)
	{
	    if (p == m_stages[i].pid)
	    {
		m_stages[i].retstatus = status;

		if (WIFEXITED(status))
		{
		    LOG_INFO("Finished exec() stage " << p << " with retcode " << WEXITSTATUS(status));
		}
		else if (WIFSIGNALED(status))
		{
		    LOG_INFO("Finished exec() stage " << p << " with signal " << WTERMSIG(status));
		}
		else
		{
		    LOG_ERROR("Error in wait(): unknown return status for pid " << p);
		}

		++donepid;
		found = true;
		break;
	    }
	}

	if (!found)
	{
	    LOG_ERROR("Error in wait(): syscall returned an unknown child pid.");
	}
    }

    LOG_INFO("Finished running pipe.");
}

// --- ExecPipe --------------------------------------------------------- //

ExecPipe::ExecPipe()
    : m_impl(new ExecPipeImpl)
{
    ++m_impl->refs();
}

ExecPipe::~ExecPipe()
{
    if (--m_impl->refs() == 0)
	delete m_impl;
}

ExecPipe::ExecPipe(const ExecPipe& ep)
    : m_impl(ep.m_impl)
{
    ++m_impl->refs();
}

ExecPipe& ExecPipe::operator=(const ExecPipe& ep)
{
    if (this != &ep)
    {
	if (--m_impl->refs() == 0)
	    delete m_impl;

	m_impl = ep.m_impl;
	++m_impl->refs();
    }
    return *this;
}

void ExecPipe::set_debug_level(enum DebugLevel dl)
{
    return m_impl->set_debug_level(dl);
}

void ExecPipe::set_debug_output(void (*output)(const char *line))
{
    return m_impl->set_debug_output(output);
}

void ExecPipe::set_input_fd(int fd)
{
    return m_impl->set_input_fd(fd);
}

void ExecPipe::set_input_file(const char* path)
{
    return m_impl->set_input_file(path);
}

void ExecPipe::set_input_string(const std::string* input)
{
    return m_impl->set_input_string(input);
}

void ExecPipe::set_input_source(PipeSource* source)
{
    return m_impl->set_input_source(source);
}
   
void ExecPipe::set_output_fd(int fd)
{
    return m_impl->set_output_fd(fd);
}

void ExecPipe::set_output_file(const char* path, int mode)
{
    return m_impl->set_output_file(path, mode);
}

void ExecPipe::set_output_string(std::string* output)
{
    return m_impl->set_output_string(output);
}

void ExecPipe::set_output_sink(PipeSink* sink)
{
    return m_impl->set_output_sink(sink);
}

unsigned int ExecPipe::size() const
{
    return m_impl->size();
}

void ExecPipe::add_exec(const char* prog)
{
    return m_impl->add_exec(prog);
}

void ExecPipe::add_exec(const char* prog, const char* arg1)
{
    return m_impl->add_exec(prog, arg1);
}

void ExecPipe::add_exec(const char* prog, const char* arg1, const char* arg2)
{
    return m_impl->add_exec(prog, arg1, arg2);
}

void ExecPipe::add_exec(const char* prog, const char* arg1, const char* arg2, const char* arg3)
{
    return m_impl->add_exec(prog, arg1, arg2, arg3);
}

void ExecPipe::add_exec(const std::vector<std::string>* args)
{
    return m_impl->add_exec(args);
}

void ExecPipe::add_execp(const char* prog)
{
    return m_impl->add_execp(prog);
}

void ExecPipe::add_execp(const char* prog, const char* arg1)
{
    return m_impl->add_execp(prog, arg1);
}

void ExecPipe::add_execp(const char* prog, const char* arg1, const char* arg2)
{
    return m_impl->add_execp(prog, arg1, arg2);
}

void ExecPipe::add_execp(const char* prog, const char* arg1, const char* arg2, const char* arg3)
{
    return m_impl->add_execp(prog, arg1, arg2, arg3);
}

void ExecPipe::add_execp(const std::vector<std::string>* args)
{
    return m_impl->add_execp(args);
}

void ExecPipe::add_exece(const char* path,
			 const std::vector<std::string>* args,
			 const std::vector<std::string>* env)
{
    return m_impl->add_exece(path, args, env);
}

void ExecPipe::add_function(PipeFunction* func)
{	
    return m_impl->add_function(func);
}

ExecPipe& ExecPipe::run()
{
    m_impl->run();
    return *this;
}

int ExecPipe::get_return_status(unsigned int stageid) const
{
    return m_impl->get_return_status(stageid);
}

int ExecPipe::get_return_code(unsigned int stageid) const
{
    return m_impl->get_return_code(stageid);
}

int ExecPipe::get_return_signal(unsigned int stageid) const
{
    return m_impl->get_return_signal(stageid);
}

bool ExecPipe::all_return_codes_zero() const
{
    return m_impl->all_return_codes_zero();
}

// --- PipeSource ------------------------------------------------------- //

PipeSource::PipeSource()
    : m_impl(NULL)
{
}

void PipeSource::write(const void* data, unsigned int datalen)
{
    assert(m_impl);
    return m_impl->input_source_write(data, datalen);
}

// --- PipeFunction ----------------------------------------------------- //

PipeFunction::PipeFunction()
    : m_impl(NULL), m_stageid(0)
{
}

void PipeFunction::write(const void* data, unsigned int datalen)
{
    assert(m_impl);
    return m_impl->stage_function_write(m_stageid, data, datalen);
}

// ---------------------------------------------------------------------- //

} // namespace stx
