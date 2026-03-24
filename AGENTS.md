* Assume you already in googleplex-android directory
* The environment you are working in doesn't allow you to run version control
  commands,  so don't try to use git or vsc commands
* Build only using build_target tool, and never build using terminal tool.
* If unsure build ndk_translation_all and ndk_translation_run_host_tests targets
with lunch set to cf_x86_64_phone-trunk_staging-userdebug
* Command line utilities: Strictly prefer fdfind to find and ag to grep
* When modifying module lists in blueprints or header lists in sources make
sure to sort the result, to comply with the existing style