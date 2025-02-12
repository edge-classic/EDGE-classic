// Copyright (c) 2008-2022 the Urho3D project.
// Copyright (c) 2024-2024 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

#include "epi_str_hash.h"

#include "epi.h"
#ifdef EDGE_EXTRA_CHECKS
#include "epi_str_compare.h"
#endif
#include "stb_sprintf.h"

namespace epi
{

static const int  CONVERSION_BUFFER_LENGTH = 128;
static const std::string EMPTY_STRING{};

const StringHash StringHash::kEmpty{""};

#ifdef EDGE_EXTRA_CHECKS
std::unordered_map<StringHash, std::string> StringHash::global_hash_registry_;
void StringHash::Register(StringHash hash, const std::string_view &str)
{
    auto iter = global_hash_registry_.find(hash);
    if (iter == global_hash_registry_.end())
    {
        global_hash_registry_.emplace(hash, std::string(str));
    }
    else if (epi::StringCaseCompareASCII(iter->second, str) != 0)
    {
        FatalError("StringHash collision detected! Both \"%s\" and \"%s\" have hash #%s",
            std::string(str).c_str(), iter->second.c_str(), hash.ToString().c_str());
    }
}
void StringHash::Register(const char *str)
{
    Register(StringHash(str), str);
}
std::string StringHash::GetRegistered(StringHash hash)
{
    auto iter = global_hash_registry_.find(hash);
    return iter == global_hash_registry_.end() ? EMPTY_STRING : iter->second;
}
const std::unordered_map<StringHash, std::string> &StringHash::GetHashRegistry()
{
    return global_hash_registry_;
}
#endif

std::string StringHash::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    stbsp_sprintf(tempBuffer, "%08X", value_);
    return std::string(tempBuffer);
}

std::string StringHash::ToDebugString() const
{
#ifdef EDGE_EXTRA_CHECKS
    return epi::StringFormat("#%s '%s'", ToString().c_str(), Reverse().c_str());
#else
    return epi::StringFormat("#%s", ToString().c_str());
#endif
}

std::string StringHash::Reverse() const
{
#ifdef EDGE_EXTRA_CHECKS
    const std::string copy = GetRegistered(*this);
    return copy;
#else
    return EMPTY_STRING;
#endif
}

#ifdef EDGE_EXTRA_CHECKS
// Register strings that were created via the EPI_KNOWN_STRINGHASH macro
// in various parts of the program
void StringHash::RegisterKnownStrings()
{
    // UDMF
    StringHash::Register("SPECIAL");
    StringHash::Register("ID");
    StringHash::Register("X");
    StringHash::Register("Y");
    StringHash::Register("SECTOR");
    StringHash::Register("THING");
    StringHash::Register("VERTEX");
    StringHash::Register("LINEDEF");
    StringHash::Register("SIDEDEF");
    StringHash::Register("ZFLOOR");
    StringHash::Register("ZCEILING");
    StringHash::Register("V1");
    StringHash::Register("V2");
    StringHash::Register("SIDEFRONT");
    StringHash::Register("SIDEBACK");
    StringHash::Register("BLOCKING");
    StringHash::Register("BLOCKMONSTERS");
    StringHash::Register("TWOSIDED");
    StringHash::Register("DONTPEGTOP");
    StringHash::Register("DONTPEGBOTTOM");
    StringHash::Register("SECRET");
    StringHash::Register("BLOCKSOUND");
    StringHash::Register("DONTDRAW");
    StringHash::Register("MAPPED");
    StringHash::Register("PASSUSE");
    StringHash::Register("BLOCKPLAYERS");
    StringHash::Register("BLOCKSIGHT");
    StringHash::Register("OFFSETX");
    StringHash::Register("OFFSETY");
    StringHash::Register("OFFSETX_BOTTOM");
    StringHash::Register("OFFSETX_MID");
    StringHash::Register("OFFSETX_TOP");
    StringHash::Register("OFFSETY_BOTTOM");
    StringHash::Register("OFFSETY_MID");
    StringHash::Register("OFFSETY_TOP");
    StringHash::Register("SCALEX_BOTTOM");
    StringHash::Register("SCALEX_MID");
    StringHash::Register("SCALEX_TOP");
    StringHash::Register("SCALEY_BOTTOM");
    StringHash::Register("SCALEY_MID");
    StringHash::Register("SCALEY_TOP");
    StringHash::Register("TEXTURETOP");
    StringHash::Register("TEXTUREBOTTOM");
    StringHash::Register("TEXTUREMIDDLE");
    StringHash::Register("HEIGHTFLOOR");
    StringHash::Register("HEIGHTCEILING");
    StringHash::Register("TEXTUREFLOOR");
    StringHash::Register("TEXTURECEILING");
    StringHash::Register("LIGHTLEVEL");
    StringHash::Register("LIGHTCOLOR");
    StringHash::Register("FADECOLOR");
    StringHash::Register("FOGDENSITY");
    StringHash::Register("XPANNINGFLOOR");
    StringHash::Register("YPANNINGFLOOR");
    StringHash::Register("XPANNINGCEILING");
    StringHash::Register("YPANNINGCEILING");
    StringHash::Register("XSCALEFLOOR");
    StringHash::Register("YSCALEFLOOR");
    StringHash::Register("XSCALECEILING");
    StringHash::Register("YSCALECEILING");
    StringHash::Register("ALPHAFLOOR");
    StringHash::Register("ALPHACEILING");
    StringHash::Register("ROTATIONFLOOR");
    StringHash::Register("ROTATIONCEILING");
    StringHash::Register("GRAVITY");
    StringHash::Register("REVERBPRESET");
    StringHash::Register("HEIGHT");
    StringHash::Register("ANGLE");
    StringHash::Register("TYPE");
    StringHash::Register("SKILL1");
    StringHash::Register("SKILL2");
    StringHash::Register("SKILL3");
    StringHash::Register("SKILL4");
    StringHash::Register("SKILL5");
    StringHash::Register("AMBUSH");
    StringHash::Register("SINGLE");
    StringHash::Register("DM");
    StringHash::Register("COOP");
    StringHash::Register("FRIEND");
    StringHash::Register("HEALTH");
    StringHash::Register("ALPHA");
    StringHash::Register("SCALE");
    StringHash::Register("SCALEX");
    StringHash::Register("SCALEY");

    // UMAPINFO
    StringHash::Register("LEVELNAME");
    StringHash::Register("LABEL");
    StringHash::Register("NEXT");
    StringHash::Register("NEXTSECRET");
    StringHash::Register("LEVELPIC");
    StringHash::Register("SKYTEXTURE");
    StringHash::Register("MUSIC");
    StringHash::Register("ENDPIC");
    StringHash::Register("ENDCAST");
    StringHash::Register("ENDBUNNY");
    StringHash::Register("ENDGAME");
    StringHash::Register("EXITPIC");
    StringHash::Register("ENTERPIC");
    StringHash::Register("NOINTERMISSION");
    StringHash::Register("PARTIME");
    StringHash::Register("INTERTEXT");
    StringHash::Register("INTERTEXTSECRET");
    StringHash::Register("INTERBACKDROP");
    StringHash::Register("INTERMUSIC");
    StringHash::Register("EPISODE");
    StringHash::Register("BOSSACTION");
    StringHash::Register("AUTHOR");
    StringHash::Register("DOOMPLAYER");
    StringHash::Register("ZOMBIEMAN");
    StringHash::Register("SHOTGUNGUY");
    StringHash::Register("ARCHVILE");
    StringHash::Register("ARCHVILEFIRE");
    StringHash::Register("REVENANT");
    StringHash::Register("REVENANTTRACER");
    StringHash::Register("REVENANTTRACERSMOKE");
    StringHash::Register("FATSO");
    StringHash::Register("FATSHOT");
    StringHash::Register("CHAINGUNGUY");
    StringHash::Register("DOOMIMP");
    StringHash::Register("DEMON");
    StringHash::Register("SPECTRE");
    StringHash::Register("CACODEMON");
    StringHash::Register("BARONOFHELL");
    StringHash::Register("BARONBALL");
    StringHash::Register("HELLKNIGHT");
    StringHash::Register("LOSTSOUL");
    StringHash::Register("SPIDERMASTERMIND");
    StringHash::Register("ARACHNOTRON");
    StringHash::Register("CYBERDEMON");
    StringHash::Register("PAINELEMENTAL");
    StringHash::Register("WOLFENSTEINSS");
    StringHash::Register("COMMANDERKEEN");
    StringHash::Register("BOSSBRAIN");
    StringHash::Register("BOSSEYE");
    StringHash::Register("BOSSTARGET");
    StringHash::Register("SPAWNSHOT");
    StringHash::Register("SPAWNFIRE");
    StringHash::Register("EXPLOSIVEBARREL");
    StringHash::Register("DOOMIMPBALL");
    StringHash::Register("CACODEMONBALL");
    StringHash::Register("ROCKET");
    StringHash::Register("PLASMABALL");
    StringHash::Register("BFGBALL");
    StringHash::Register("ARACHNOTRONPLASMA");
    StringHash::Register("BULLETPUFF");
    StringHash::Register("BLOOD");
    StringHash::Register("TELEPORTFOG");
    StringHash::Register("ITEMFOG");
    StringHash::Register("TELEPORTDEST");
    StringHash::Register("BFGEXTRA");
    StringHash::Register("GREENARMOR");
    StringHash::Register("BLUEARMOR");
    StringHash::Register("HEALTHBONUS");
    StringHash::Register("ARMORBONUS");
    StringHash::Register("BLUECARD");
    StringHash::Register("REDCARD");
    StringHash::Register("YELLOWCARD");
    StringHash::Register("YELLOWSKULL");
    StringHash::Register("REDSKULL");
    StringHash::Register("BLUESKULL");
    StringHash::Register("STIMPACK");
    StringHash::Register("MEDIKIT");
    StringHash::Register("SOULSPHERE");
    StringHash::Register("INVULNERABILITYSPHERE");
    StringHash::Register("BERSERK");
    StringHash::Register("BLURSPHERE");
    StringHash::Register("RADSUIT");
    StringHash::Register("ALLMAP");
    StringHash::Register("INFRARED");
    StringHash::Register("MEGASPHERE");
    StringHash::Register("CLIP");
    StringHash::Register("CLIPBOX");
    StringHash::Register("ROCKETAMMO");
    StringHash::Register("ROCKETBOX");
    StringHash::Register("CELL");
    StringHash::Register("CELLPACK");
    StringHash::Register("SHELL");
    StringHash::Register("SHELLBOX");
    StringHash::Register("BACKPACK");
    StringHash::Register("BFG9000");
    StringHash::Register("CHAINGUN");
    StringHash::Register("CHAINSAW");
    StringHash::Register("ROCKETLAUNCHER");
    StringHash::Register("PLASMARIFLE");
    StringHash::Register("SHOTGUN");
    StringHash::Register("SUPERSHOTGUN");
    StringHash::Register("TECHLAMP");
    StringHash::Register("TECHLAMP2");
    StringHash::Register("COLUMN");
    StringHash::Register("TALLGREENCOLUMN");
    StringHash::Register("SHORTGREENCOLUMN");
    StringHash::Register("TALLREDCOLUMN");
    StringHash::Register("SHORTREDCOLUMN");
    StringHash::Register("SKULLCOLUMN");
    StringHash::Register("HEARTCOLUMN");
    StringHash::Register("EVILEYE");
    StringHash::Register("FLOATINGSKULL");
    StringHash::Register("TORCHTREE");
    StringHash::Register("BLUETORCH");
    StringHash::Register("GREENTORCH");
    StringHash::Register("REDTORCH");
    StringHash::Register("SHORTBLUETORCH");
    StringHash::Register("SHORTGREENTORCH");
    StringHash::Register("SHORTREDTORCH");
    StringHash::Register("STALAGTITE");
    StringHash::Register("TECHPILLAR");
    StringHash::Register("CANDLESTICK");
    StringHash::Register("CANDELABRA");
    StringHash::Register("BLOODYTWITCH");
    StringHash::Register("MEAT2");
    StringHash::Register("MEAT3");
    StringHash::Register("MEAT4");
    StringHash::Register("MEAT5");
    StringHash::Register("NONSOLIDMEAT2");
    StringHash::Register("NONSOLIDMEAT4");
    StringHash::Register("NONSOLIDMEAT3");
    StringHash::Register("NONSOLIDMEAT5");
    StringHash::Register("NONSOLIDTWITCH");
    StringHash::Register("DEADCACODEMON");
    StringHash::Register("DEADMARINE");
    StringHash::Register("DEADZOMBIEMAN");
    StringHash::Register("DEADDEMON");
    StringHash::Register("DEADLOSTSOUL");
    StringHash::Register("DEADDOOMIMP");
    StringHash::Register("DEADSHOTGUNGUY");
    StringHash::Register("GIBBEDMARINE");
    StringHash::Register("GIBBEDMARINEEXTRA");
    StringHash::Register("HEADSONASTICK");
    StringHash::Register("GIBS");
    StringHash::Register("HEADONASTICK");
    StringHash::Register("HEADCANDLES");
    StringHash::Register("DEADSTICK");
    StringHash::Register("LIVESTICK");
    StringHash::Register("BIGTREE");
    StringHash::Register("BURNINGBARREL");
    StringHash::Register("HANGNOGUTS");
    StringHash::Register("HANGBNOBRAIN");
    StringHash::Register("HANGTLOOKINGDOWN");
    StringHash::Register("HANGTSKULL");
    StringHash::Register("HANGTLOOKINGUP");
    StringHash::Register("HANGTNOBRAIN");
    StringHash::Register("COLONGIBS");
    StringHash::Register("SMALLBLOODPOOL");
    StringHash::Register("BRAINSTEM");
    StringHash::Register("POINTPUSHER");
    StringHash::Register("POINTPULLER");
    StringHash::Register("MBFHELPERDOG");
    StringHash::Register("PLASMABALL1");
    StringHash::Register("PLASMABALL2");
    StringHash::Register("EVILSCEPTRE");
    StringHash::Register("UNHOLYBIBLE");
    StringHash::Register("MUSICCHANGER");
    StringHash::Register("DEH_ACTOR_145");
    StringHash::Register("DEH_ACTOR_146");
    StringHash::Register("DEH_ACTOR_147");
    StringHash::Register("DEH_ACTOR_148");
    StringHash::Register("DEH_ACTOR_149");
    StringHash::Register("DEH_ACTOR_150");
    StringHash::Register("DEH_ACTOR_151");
    StringHash::Register("DEH_ACTOR_152");
    StringHash::Register("DEH_ACTOR_153");
    StringHash::Register("DEH_ACTOR_154");
    StringHash::Register("DEH_ACTOR_155");
    StringHash::Register("DEH_ACTOR_156");
    StringHash::Register("DEH_ACTOR_157");
    StringHash::Register("DEH_ACTOR_158");
    StringHash::Register("DEH_ACTOR_159");
    StringHash::Register("DEH_ACTOR_160");
    StringHash::Register("DEH_ACTOR_161");
    StringHash::Register("DEH_ACTOR_162");
    StringHash::Register("DEH_ACTOR_163");
    StringHash::Register("DEH_ACTOR_164");
    StringHash::Register("DEH_ACTOR_165");
    StringHash::Register("DEH_ACTOR_166");
    StringHash::Register("DEH_ACTOR_167");
    StringHash::Register("DEH_ACTOR_168");
    StringHash::Register("DEH_ACTOR_169");
    StringHash::Register("DEH_ACTOR_170");
    StringHash::Register("DEH_ACTOR_171");
    StringHash::Register("DEH_ACTOR_172");
    StringHash::Register("DEH_ACTOR_173");
    StringHash::Register("DEH_ACTOR_174");
    StringHash::Register("DEH_ACTOR_175");
    StringHash::Register("DEH_ACTOR_176");
    StringHash::Register("DEH_ACTOR_177");
    StringHash::Register("DEH_ACTOR_178");
    StringHash::Register("DEH_ACTOR_179");
    StringHash::Register("DEH_ACTOR_180");
    StringHash::Register("DEH_ACTOR_181");
    StringHash::Register("DEH_ACTOR_182");
    StringHash::Register("DEH_ACTOR_183");
    StringHash::Register("DEH_ACTOR_184");
    StringHash::Register("DEH_ACTOR_185");
    StringHash::Register("DEH_ACTOR_186");
    StringHash::Register("DEH_ACTOR_187");
    StringHash::Register("DEH_ACTOR_188");
    StringHash::Register("DEH_ACTOR_189");
    StringHash::Register("DEH_ACTOR_190");
    StringHash::Register("DEH_ACTOR_191");
    StringHash::Register("DEH_ACTOR_192");
    StringHash::Register("DEH_ACTOR_193");
    StringHash::Register("DEH_ACTOR_194");
    StringHash::Register("DEH_ACTOR_195");
    StringHash::Register("DEH_ACTOR_196");
    StringHash::Register("DEH_ACTOR_197");
    StringHash::Register("DEH_ACTOR_198");
    StringHash::Register("DEH_ACTOR_199");
    StringHash::Register("DEH_ACTOR_200");
    StringHash::Register("DEH_ACTOR_201");
    StringHash::Register("DEH_ACTOR_202");
    StringHash::Register("DEH_ACTOR_203");
    StringHash::Register("DEH_ACTOR_204");
    StringHash::Register("DEH_ACTOR_205");
    StringHash::Register("DEH_ACTOR_206");
    StringHash::Register("DEH_ACTOR_207");
    StringHash::Register("DEH_ACTOR_208");
    StringHash::Register("DEH_ACTOR_209");
    StringHash::Register("DEH_ACTOR_210");
    StringHash::Register("DEH_ACTOR_211");
    StringHash::Register("DEH_ACTOR_212");
    StringHash::Register("DEH_ACTOR_213");
    StringHash::Register("DEH_ACTOR_214");
    StringHash::Register("DEH_ACTOR_215");
    StringHash::Register("DEH_ACTOR_216");
    StringHash::Register("DEH_ACTOR_217");
    StringHash::Register("DEH_ACTOR_218");
    StringHash::Register("DEH_ACTOR_219");
    StringHash::Register("DEH_ACTOR_220");
    StringHash::Register("DEH_ACTOR_221");
    StringHash::Register("DEH_ACTOR_222");
    StringHash::Register("DEH_ACTOR_223");
    StringHash::Register("DEH_ACTOR_224");
    StringHash::Register("DEH_ACTOR_225");
    StringHash::Register("DEH_ACTOR_226");
    StringHash::Register("DEH_ACTOR_227");
    StringHash::Register("DEH_ACTOR_228");
    StringHash::Register("DEH_ACTOR_229");
    StringHash::Register("DEH_ACTOR_230");
    StringHash::Register("DEH_ACTOR_231");
    StringHash::Register("DEH_ACTOR_232");
    StringHash::Register("DEH_ACTOR_233");
    StringHash::Register("DEH_ACTOR_234");
    StringHash::Register("DEH_ACTOR_235");
    StringHash::Register("DEH_ACTOR_236");
    StringHash::Register("DEH_ACTOR_237");
    StringHash::Register("DEH_ACTOR_238");
    StringHash::Register("DEH_ACTOR_239");
    StringHash::Register("DEH_ACTOR_240");
    StringHash::Register("DEH_ACTOR_241");
    StringHash::Register("DEH_ACTOR_242");
    StringHash::Register("DEH_ACTOR_243");
    StringHash::Register("DEH_ACTOR_244");
    StringHash::Register("DEH_ACTOR_245");
    StringHash::Register("DEH_ACTOR_246");
    StringHash::Register("DEH_ACTOR_247");
    StringHash::Register("DEH_ACTOR_248");
    StringHash::Register("DEH_ACTOR_249");

    // DDF
    StringHash::Register("ROOMSIZE");
    StringHash::Register("DAMPINGLEVEL");
    StringHash::Register("WETLEVEL");
    StringHash::Register("DRYLEVEL");
    StringHash::Register("REVERBWIDTH");
    StringHash::Register("REVERBGAIN");
}
#endif

} // namespace epi
