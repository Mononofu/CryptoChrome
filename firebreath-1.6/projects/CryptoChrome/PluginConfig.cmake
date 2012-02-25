#/**********************************************************\ 
#
# Auto-Generated Plugin Configuration file
# for CryptoChrome
#
#\**********************************************************/

set(PLUGIN_NAME "CryptoChrome")
set(PLUGIN_PREFIX "CCH")
set(COMPANY_NAME "Furidamu")

# ActiveX constants:
set(FBTYPELIB_NAME CryptoChromeLib)
set(FBTYPELIB_DESC "CryptoChrome 1.0 Type Library")
set(IFBControl_DESC "CryptoChrome Control Interface")
set(FBControl_DESC "CryptoChrome Control Class")
set(IFBComJavascriptObject_DESC "CryptoChrome IComJavascriptObject Interface")
set(FBComJavascriptObject_DESC "CryptoChrome ComJavascriptObject Class")
set(IFBComEventSource_DESC "CryptoChrome IFBComEventSource Interface")
set(AXVERSION_NUM "1")

# NOTE: THESE GUIDS *MUST* BE UNIQUE TO YOUR PLUGIN/ACTIVEX CONTROL!  YES, ALL OF THEM!
set(FBTYPELIB_GUID 4237f307-29bd-50be-9f4c-05b7157e7c4a)
set(IFBControl_GUID 04a1e0f3-fc46-5dc0-b408-3c26e04de2f2)
set(FBControl_GUID e110484b-9e63-5798-9785-546794246b2f)
set(IFBComJavascriptObject_GUID 30cc91e5-6d22-5e23-9da9-1dbd47212cd8)
set(FBComJavascriptObject_GUID 812d3174-1721-5fe5-8e34-aef61f1169b1)
set(IFBComEventSource_GUID b5b9fe38-64a9-5ad9-87d6-1b0c19d4acd4)

# these are the pieces that are relevant to using it from Javascript
set(ACTIVEX_PROGID "Furidamu.CryptoChrome")
set(MOZILLA_PLUGINID "furidamu.org/CryptoChrome")

# strings
set(FBSTRING_CompanyName "Furidamu")
set(FBSTRING_FileDescription "a GnuPG interface for Chrome")
set(FBSTRING_PLUGIN_VERSION "1.0.0.0")
set(FBSTRING_LegalCopyright "Copyright 2012 Furidamu")
set(FBSTRING_PluginFileName "np${PLUGIN_NAME}.dll")
set(FBSTRING_ProductName "CryptoChrome")
set(FBSTRING_FileExtents "")
set(FBSTRING_PluginName "CryptoChrome")
set(FBSTRING_MIMEType "application/x-cryptochrome")

# Uncomment this next line if you're not planning on your plugin doing
# any drawing:

set (FB_GUI_DISABLED 1)

# Mac plugin settings. If your plugin does not draw, set these all to 0
set(FBMAC_USE_QUICKDRAW 0)
set(FBMAC_USE_CARBON 0)
set(FBMAC_USE_COCOA 0)
set(FBMAC_USE_COREGRAPHICS 0)
set(FBMAC_USE_COREANIMATION 0)
set(FBMAC_USE_INVALIDATINGCOREANIMATION 0)

# If you want to register per-machine on Windows, uncomment this line
#set (FB_ATLREG_MACHINEWIDE 1)
