#include "activity/mods_activity.hpp"

#include <borealis.hpp>

// Static members to share game info with tabs
const char* ModsActivity::currentGameName = "";
const char* ModsActivity::currentGamePath = "";
const char* ModsActivity::currentTitleId = "";

// Note: This activity class is not directly instantiated.
// Instead, the XML view is loaded directly via createFromXMLResource in mod_install_tab.cpp
// The static members above are used to pass game information to the tabs.
