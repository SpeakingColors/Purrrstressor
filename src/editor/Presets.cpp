#include "Presets.h"
using namespace juce;

namespace presets
{
static Preset mk (const char* name, const char* type, StringArray tags, std::initializer_list<std::pair<const char*, double>> over)
{
    Preset p; p.name = name; p.type = type; p.tags = tags; p.factory = true;
    for (auto& o : over)
        for (int i = 0; i < kCount; ++i)
            if (String (o.first) == keys[i]) p.v[i] = o.second;
    return p;
}

const std::vector<Preset>& factory()
{
    static const std::vector<Preset> list {
        mk ("Start Here 5555", "Bus", { "start", "general" }, {}),
        mk ("Vocal Smooth", "Vocal", { "smooth", "tracking" }, { { "input", 6 }, { "attack", 5 }, { "release", 4 }, { "detHP", 1 } }),
        mk ("Vocal Bite", "Vocal", { "aggressive" }, { { "input", 7 }, { "attack", 3 }, { "release", 1 }, { "audioHP", 1 }, { "dist", 1 } }),
        mk ("Bass Tight", "Bass", { "tight" }, { { "ratio", 3 }, { "input", 6 }, { "attack", 5 }, { "release", 4 }, { "dist", 1 } }),
        mk ("Opto Kick", "Drum", { "kick", "opto" }, { { "ratio", 5 }, { "input", 6 }, { "attack", 10 }, { "release", 0 }, { "detHP", 1 } }),
        mk ("Room Nuke", "Drum", { "room", "limiting" }, { { "ratio", 7 }, { "input", 10 }, { "attack", 6 }, { "release", 2.5 }, { "output", 6 } }),
        mk ("Snare Pop", "Drum", { "snare" }, { { "ratio", 2 }, { "input", 6 }, { "attack", 5 }, { "release", 5 }, { "output", 6 } }),
        mk ("Hard Kick Punch", "Drum", { "kick", "hard dance" }, { { "ratio", 3 }, { "input", 6 }, { "attack", 6 }, { "release", 2 }, { "detHP", 1 }, { "dist", 2 } }),
        mk ("Tape Warmth", "FX", { "tape", "gentle" }, { { "ratio", 1 }, { "input", 4 }, { "attack", 1 }, { "release", 1 }, { "dist", 2 } }),
        mk ("Acoustic Glass", "Guitar", { "acoustic" }, { { "input", 7 }, { "attack", 2 }, { "release", 5 }, { "output", 7 }, { "detHP", 1 }, { "audioHP", 1 } }),
        mk ("Piano Hold", "Keys", { "opto", "sustain" }, { { "ratio", 5 }, { "input", 6 }, { "attack", 5 }, { "release", 5 } }),
        mk ("Parallel Squash", "Bus", { "parallel", "heavy" }, { { "ratio", 6 }, { "input", 8 }, { "attack", 3 }, { "release", 3 }, { "mix", 40 } }),
        mk ("Warm 1:1", "FX", { "saturation" }, { { "ratio", 0 }, { "input", 6 }, { "dist", 1 } }),
        mk ("Clean Glue", "Bus", { "glue", "mix bus" }, { { "ratio", 1 }, { "input", 4 }, { "attack", 7 }, { "release", 6 }, { "detHP", 1 }, { "link", 1 } }),
    };
    return list;
}

File userFolder()
{
   #if JUCE_LINUX || JUCE_BSD
    const String xdg = SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const File base = (xdg.isNotEmpty() && File::isAbsolutePath (xdg)) ? File (xdg)
                                                                       : File::getSpecialLocation (File::userHomeDirectory).getChildFile (".local/share");
   #else
    const File base = File::getSpecialLocation (File::userApplicationDataDirectory); // %APPDATA% or ~/Library/Application Support
   #endif
    return base.getChildFile ("Studio 87").getChildFile ("Purrrstressor").getChildFile ("presets");
}

bool valid (const var& o)
{
    auto* obj = o.getDynamicObject();
    if (obj == nullptr) return false;
    const var name = obj->getProperty ("name"), pv = obj->getProperty ("p");
    if (! name.isString() || name.toString().isEmpty() || name.toString().length() > 60) return false;
    auto* po = pv.getDynamicObject();
    if (po == nullptr) return false;
    for (auto& nv : po->getProperties())
        for (int i = 0; i < kCount; ++i)
            if (nv.name.toString() == keys[i])
            {
                if (! (nv.value.isDouble() || nv.value.isInt() || nv.value.isInt64())) return false;
                const double x = (double) nv.value;
                if (! std::isfinite (x) || x < lo[i] || x > hi[i]) return false;
            }
    return true;
}

Preset fromVar (const var& o)
{
    Preset p;
    p.name = o["name"].toString().substring (0, 60);
    const String t = o["type"].toString();
    p.type = "Bus";
    for (auto* ty : types) if (t == ty) p.type = t;
    if (auto* arr = o["tags"].getArray())
        for (auto& tg : *arr)
            if (tg.isString() && p.tags.size() < 8) p.tags.add (tg.toString().substring (0, 20));
    if (auto* po = o["p"].getDynamicObject())
        for (int i = 0; i < kCount; ++i)
            if (po->hasProperty (keys[i])) p.v[i] = (double) po->getProperty (keys[i]);
    return p;
}

var toVar (const Preset& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("type", p.type);
    Array<var> tg; for (auto& t : p.tags) tg.add (t);
    o->setProperty ("tags", tg);
    auto* pv = new DynamicObject();
    for (int i = 0; i < kCount; ++i) pv->setProperty (keys[i], p.v[i]);
    o->setProperty ("p", var (pv));
    return var (o);
}

static String fileNameFor (const String& name)
{
    String s = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-+.");
    s = s.trim().replaceCharacters (" ", "_");
    if (s.isEmpty()) s = "preset";
    return s.substring (0, 60) + "-" + String::toHexString (name.hashCode()) + ".json";
}

Store::Store() { reload(); }

void Store::reload()
{
    mine.clear();
    auto dir = userFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        if (f.getSize() > 20000) continue;
        const var o = JSON::parse (f);
        if (! valid (o)) continue;
        Preset p = fromVar (o);
        bool clash = false;
        for (auto& x : factory()) if (x.name == p.name) clash = true;
        for (auto& x : mine) if (x.name == p.name) clash = true;
        if (! clash) mine.push_back (p);
    }
    std::sort (mine.begin(), mine.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

std::vector<Preset> Store::all() const
{
    std::vector<Preset> r = factory();
    r.insert (r.end(), mine.begin(), mine.end());
    return r;
}

const Preset* Store::find (const String& name) const
{
    for (auto& p : factory()) if (p.name == name) return &p;
    for (auto& p : mine) if (p.name == name) return &p;
    return nullptr;
}

bool Store::save (const Preset& p, const String& replaceName)
{
    auto dir = userFolder();
    if (! dir.createDirectory().wasOk()) return false;
    const String old = replaceName.isEmpty() ? p.name : replaceName;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        const var o = JSON::parse (f);
        if (o["name"].toString() == old || o["name"].toString() == p.name) f.deleteFile();
    }
    Preset q = p; q.factory = false;
    if (! dir.getChildFile (fileNameFor (q.name)).replaceWithText (JSON::toString (toVar (q), false))) return false;
    reload();
    return true;
}

bool Store::remove (const String& name)
{
    bool ok = false;
    for (auto& f : userFolder().findChildFiles (File::findFiles, false, "*.json"))
        if (JSON::parse (f)["name"].toString() == name) ok = f.deleteFile() || ok;
    reload();
    return ok;
}

String Store::exportJson (const std::vector<Preset>& list) const
{
    auto* root = new DynamicObject();
    root->setProperty ("purrrstressor", 1);
    Array<var> arr; for (auto& p : list) arr.add (toVar (p));
    root->setProperty ("presets", arr);
    return JSON::toString (var (root), false);
}

int Store::importJson (const String& text)
{
    const var o = JSON::parse (text);
    if (! o.isObject() || (int) o["purrrstressor"] != 1) return -1;
    auto* arr = o["presets"].getArray();
    if (arr == nullptr || arr->isEmpty() || arr->size() > 200) return -1;
    for (auto& x : *arr) if (! valid (x)) return -1;
    for (auto& x : *arr)
    {
        Preset p = fromVar (x);
        for (auto& f : factory()) if (f.name == p.name) p.name += " (imported)";
        save (p);
    }
    return arr->size();
}
} // namespace presets
