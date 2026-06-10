#pragma once

#include <mq/Plugin.h>

#include <cmath>
#include <string>

namespace myui
{
// Bearing from the player to a world position (spawnX/spawnY), relative to the
// player's facing: 0 = directly ahead, clockwise positive, range [-180, 180].
// Feeds DrawDirectionRing. The atan2 bearing-to and the raw facing (Heading *
// 360/512) share EQ's native heading frame (the same pairing MQ's IsMobFleeing
// uses, MQ2Inlines.h). The subtraction is facing - bearingTo (not the reverse):
// in EQ's frame that orientation makes the marker swing opposite your turn and
// puts a spawn on your left on the left of the ring. Reversing it negates the
// result - the marker spins with you and left/right swap.
inline float RelativeBearingDeg(float spawnX, float spawnY)
{
	if (!pLocalPlayer)
	{
		return 0.0f;
	}
	float headingTo = atan2f(pLocalPlayer->Y - spawnY, spawnX - pLocalPlayer->X) * 180.0f / 3.14159265358979323846f + 90.0f;
	if (headingTo < 0.0f)
	{
		headingTo += 360.0f;
	}
	else if (headingTo >= 360.0f)
	{
		headingTo -= 360.0f;
	}
	float rel = pLocalPlayer->Heading * 0.703125f - headingTo;
	while (rel < -180.0f)
	{
		rel += 360.0f;
	}
	while (rel > 180.0f)
	{
		rel -= 360.0f;
	}
	return rel;
}

// Short race code for a race id. Used to build anonymized names — these codes are
// intentionally not valid character names (see MaskedCode). Unknown -> "UNK".
inline const char* RaceShort(int raceId)
{
	switch (raceId)
	{
	case 1:   return "HUM"; // Human
	case 2:   return "BAR"; // Barbarian
	case 3:   return "ERU"; // Erudite
	case 4:   return "ELF"; // Wood Elf
	case 5:   return "HIE"; // High Elf
	case 6:   return "DEF"; // Dark Elf
	case 7:   return "HEF"; // Half Elf
	case 8:   return "DWF"; // Dwarf
	case 9:   return "TRL"; // Troll
	case 10:  return "OGR"; // Ogre
	case 11:  return "HFL"; // Halfling
	case 12:  return "GNM"; // Gnome
	case 128: return "IKS"; // Iksar
	case 130: return "VAH"; // Vah Shir
	case 330: return "FRG"; // Froglok
	case 522: return "DRK"; // Drakkin
	default:  return "UNK";
	}
}

// Anonymized stand-in for a character name: RACE_CLASS_LEVEL (e.g. "HIE_WIZ_65").
// classShort is the uppercase 3-letter class code (GetClassThreeLetterCode). The
// result is deliberately not a legal character name, so it's safe to show on a
// stream/recording without exposing the real player to name-report griefing.
inline std::string MaskedCode(int raceId, const std::string& classShort, int level)
{
	const std::string cls = classShort.empty() ? std::string("UNK") : classShort;
	return std::string(RaceShort(raceId)) + "_" + cls + "_" + std::to_string(level);
}

inline std::string TrimName(const char* name)
{
	if (!name)
	{
		return std::string();
	}

	std::string s(name);
	size_t pos = s.find_first_of("\r\n");
	if (pos != std::string::npos)
	{
		s.erase(pos);
	}
	return s;
}

inline int ColumnsForWidth(float avail, float cellSize)
{
	int cols = static_cast<int>(avail / (cellSize + 6.0f));
	return cols < 1 ? 1 : cols;
}
} // namespace myui
