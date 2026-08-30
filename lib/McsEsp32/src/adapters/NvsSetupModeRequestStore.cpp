#ifdef ARDUINO

#include "NvsSetupModeRequestStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "mcs-boot";
    constexpr const char* kKey = "wsetup";
}

void NvsSetupModeRequestStore::requestOnNextBoot()
{
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putBool(kKey, true);
    prefs.end();
}

bool NvsSetupModeRequestStore::consumeRequest()
{
    Preferences prefs;
    prefs.begin(kNamespace, false);
    const bool pending = prefs.getBool(kKey, false);
    if (pending)
    {
        prefs.putBool(kKey, false);
    }
    prefs.end();
    return pending;
}

#endif
