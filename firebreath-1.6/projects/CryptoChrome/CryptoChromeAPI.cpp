/**********************************************************\

  Auto-generated CryptoChromeAPI.cpp

\**********************************************************/

#include "JSObject.h"
#include "variant_list.h"
#include "DOM/Document.h"
#include "global/config.h"
#include "stx-execpipe.h"
#include <stdexcept>
#include <iostream>

#include "CryptoChromeAPI.h"

///////////////////////////////////////////////////////////////////////////////
/// @fn FB::variant CryptoChromeAPI::echo(const FB::variant& msg)
///
/// @brief  Echos whatever is passed from Javascript.
///         Go ahead and change it. See what happens!
///////////////////////////////////////////////////////////////////////////////
FB::variant CryptoChromeAPI::echo(const FB::variant& msg)
{
    static int n(0);
    fire_echo("So far, you clicked this many times: ", n++);

    // return "foobar";
    return msg;
}

///////////////////////////////////////////////////////////////////////////////
/// @fn CryptoChromePtr CryptoChromeAPI::getPlugin()
///
/// @brief  Gets a reference to the plugin that was passed in when the object
///         was created.  If the plugin has already been released then this
///         will throw a FB::script_error that will be translated into a
///         javascript exception in the page.
///////////////////////////////////////////////////////////////////////////////
CryptoChromePtr CryptoChromeAPI::getPlugin()
{
    CryptoChromePtr plugin(m_plugin.lock());
    if (!plugin) {
        throw FB::script_error("The plugin is invalid");
    }
    return plugin;
}

// Read/Write property testString
std::string CryptoChromeAPI::get_testString()
{
    return m_testString;
}

void CryptoChromeAPI::set_testString(const std::string& val)
{
    m_testString = val;
}

// Read-only property version
std::string CryptoChromeAPI::get_version()
{
    return FBSTRING_PLUGIN_VERSION;
}

void CryptoChromeAPI::testEvent()
{
    fire_test();
}


// Decrypt
std::string CryptoChromeAPI::decrypt(std::string crypt_txt)
{
    stx::ExecPipe ep;               // creates new pipe

    ep.set_input_string(&crypt_txt);
    
    std::vector<std::string> gpgargs;
    gpgargs.push_back("/usr/bin/gpg");
    gpgargs.push_back("--quiet");
    gpgargs.push_back("--no-tty"); 
    gpgargs.push_back("--decrypt");
    gpgargs.push_back("--use-agent");
    ep.add_execp(&gpgargs);

    std::string output;
    ep.set_output_string(&output);
    

    try {
        ep.run();
    }
    catch (std::runtime_error &e) {
        return e.what();
    }

    return output;
}

std::string CryptoChromeAPI::encrypt(std::string recipient, std::string clear_txt)
{
    stx::ExecPipe ep;

    ep.set_input_string(&clear_txt);

    std::vector<std::string> gpgargs;
    gpgargs.push_back("/usr/bin/gpg");
    gpgargs.push_back("--encrypt");
    gpgargs.push_back("-quiet");
    gpgargs.push_back("--no-tty"); 
    gpgargs.push_back("--always-trust");    // maybe remove this?
    gpgargs.push_back("--armor");
    gpgargs.push_back("--recipient");
    gpgargs.push_back(recipient);   // email of the recipient
    ep.add_execp(&gpgargs);

    std::string output;
    ep.set_output_string(&output);

    try {
        ep.run();
    }
    catch (std::runtime_error &e) {
        return e.what();
    }

    return output;
}