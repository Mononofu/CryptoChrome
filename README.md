		  _____                  _         _____ _                              
		 / ____|                | |       / ____| |                             
		| |     _ __ _   _ _ __ | |_ ___ | |    | |__  _ __ ___  _ __ ___   ___ 
		| |    | '__| | | | '_ \| __/ _ \| |    | '_ \| '__/ _ \| '_ ` _ \ / _ \
		| |____| |  | |_| | |_) | || (_) | |____| | | | | | (_) | | | | | |  __/
		 \_____|_|   \__, | .__/ \__\___/ \_____|_| |_|_|  \___/|_| |_| |_|\___|
		              __/ | |                                                   
		             |___/|_|                                                   



CryptoChrome is a small Chrome extension to encrypt your text and emails with GnuPG. Currently, it only works on Linux. You'll also have to build the plugin yourself - it's not in the Chrome Webstore yet.

Building
========

This should be easy, just run

		./build.sh

However, make sure that you have libgtk2.0-dev and cmake installed. Of course, you'll also need make, gcc, etc.

Details
=======

For more information and some screenshots, also check out [my blog post](http://www.furidamu.org/blog/2012/02/26/protecting-your-mails-with-gnupg/) about it.