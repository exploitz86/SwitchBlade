#pragma once

// This class serves as a container for static members that share game information
// between ModGameData (in mod_install_tab) and the mod browser tabs.
// The activity itself is not directly instantiated; instead, the XML view is loaded
// directly via createFromXMLResource.
class ModsActivity {
public:
    // Static members to share game info with tabs
    static const char* currentGameName;
    static const char* currentGamePath;
    static const char* currentTitleId;
};
