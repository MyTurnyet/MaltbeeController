#ifdef ARDUINO

#include "NvsSetupModeRequestStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "mcs-boot";
    constexpr const char* kKey = "wsetup";
}

bool NvsSetupModeRequestStore::requestOnNextBoot()
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return false;
    }
    const bool ok = prefs.putBool(kKey, true) != 0;
    prefs.end();
    return ok;
}

bool NvsSetupModeRequestStore::consumeRequest()
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return false;
    }
    const bool pending = prefs.getBool(kKey, false);
    if (pending)
    {
        prefs.putBool(kKey, false);
    }
    prefs.end();
    return pending;
}

#endif
