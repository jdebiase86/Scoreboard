// Joe's picks for how each team's logo looks at small sizes (ticker,
// kickoff screen, wheel cards, full-game screen), chosen on contact sheets
// of every logo (Oct 2026). A team that isn't listed uses ESPN's
// dark-background logo as it is. Matched on ESPN's logo address: league
// folder + file name (college logos are named by ESPN team id).
// Shared by the board (sb_net.cpp) and the mock-ups (hosttest/logo_dir.h).
#pragma once
#include <stdint.h>
#include <string.h>
#include <strings.h>

enum : uint8_t {
  FIX_LIGHT = 1,      // ESPN's regular team-colour logo instead of the dark-background one
  FIX_KEYLINE = 2,    // draw a thin light outline as one clean dot-wide edge (sb_logo.cpp)
  FIX_CROP_TOP = 4,   // leave out the top 40% (the 76ers' ring of stars)
  FIX_LETTERS = 8,    // unreadable at any size: the team's letters instead
};

struct LogoFix { const char* league; const char* file; uint8_t flags; };

static const LogoFix LOGO_FIXES[] = {
    // NFL
    {"/nfl/", "buf", FIX_KEYLINE},   // Buffalo Bills
    {"/nfl/", "chi", FIX_KEYLINE},   // Chicago Bears
    {"/nfl/", "dal", FIX_KEYLINE},   // Dallas Cowboys
    {"/nfl/", "det", FIX_KEYLINE},   // Detroit Lions
    {"/nfl/", "ind", FIX_KEYLINE},   // Indianapolis Colts
    {"/nfl/", "jax", FIX_KEYLINE},   // Jacksonville Jaguars
    {"/nfl/", "lac", FIX_KEYLINE},   // Los Angeles Chargers
    {"/nfl/", "no", FIX_KEYLINE},   // New Orleans Saints
    {"/nfl/", "nyj", FIX_LIGHT},   // New York Jets
    {"/nfl/", "pit", FIX_KEYLINE},   // Pittsburgh Steelers
    // MLB
    {"/mlb/", "atl", FIX_LIGHT},   // Atlanta Braves
    {"/mlb/", "bal", FIX_LIGHT},   // Baltimore Orioles
    {"/mlb/", "bos", FIX_KEYLINE},   // Boston Red Sox
    {"/mlb/", "chc", FIX_LIGHT},   // Chicago Cubs
    {"/mlb/", "chw", FIX_LIGHT},   // Chicago White Sox
    {"/mlb/", "cin", FIX_LIGHT},   // Cincinnati Reds
    {"/mlb/", "cle", FIX_KEYLINE},   // Cleveland Guardians
    {"/mlb/", "det", FIX_LIGHT},   // Detroit Tigers
    {"/mlb/", "hou", FIX_LIGHT},   // Houston Astros
    {"/mlb/", "kc", FIX_LIGHT},   // Kansas City Royals
    {"/mlb/", "laa", FIX_LIGHT},   // Los Angeles Angels
    {"/mlb/", "lad", FIX_LIGHT},   // Los Angeles Dodgers
    {"/mlb/", "mia", FIX_LIGHT},   // Miami Marlins
    {"/mlb/", "mil", FIX_LIGHT},   // Milwaukee Brewers
    {"/mlb/", "nyy", FIX_LIGHT},   // New York Yankees
    {"/mlb/", "phi", FIX_LIGHT},   // Philadelphia Phillies
    {"/mlb/", "sd", FIX_LIGHT},   // San Diego Padres
    {"/mlb/", "sea", FIX_LIGHT},   // Seattle Mariners
    {"/mlb/", "sf", FIX_LIGHT},   // San Francisco Giants
    {"/mlb/", "stl", FIX_LIGHT},   // St. Louis Cardinals
    {"/mlb/", "tb", FIX_LIGHT},   // Tampa Bay Rays
    {"/mlb/", "tex", FIX_LIGHT},   // Texas Rangers
    {"/mlb/", "tor", FIX_LIGHT},   // Toronto Blue Jays
    {"/mlb/", "wsh", FIX_LIGHT},   // Washington Nationals
    // NHL
    {"/nhl/", "ana", FIX_LIGHT},   // Anaheim Ducks
    {"/nhl/", "cbj", FIX_LIGHT},   // Columbus Blue Jackets
    {"/nhl/", "cgy", FIX_LIGHT},   // Calgary Flames
    {"/nhl/", "chi", FIX_LIGHT},   // Chicago Blackhawks
    {"/nhl/", "col", FIX_LIGHT},   // Colorado Avalanche
    {"/nhl/", "dal", FIX_LIGHT},   // Dallas Stars
    {"/nhl/", "det", FIX_LIGHT},   // Detroit Red Wings
    {"/nhl/", "nsh", FIX_LIGHT},   // Nashville Predators
    {"/nhl/", "sj", FIX_KEYLINE},   // San Jose Sharks
    {"/nhl/", "stl", FIX_LIGHT},   // St. Louis Blues
    {"/nhl/", "tb", FIX_LIGHT},   // Tampa Bay Lightning
    {"/nhl/", "tor", FIX_LIGHT},   // Toronto Maple Leafs
    {"/nhl/", "utah", FIX_LIGHT},   // Utah Mammoth
    {"/nhl/", "van", FIX_LIGHT},   // Vancouver Canucks
    {"/nhl/", "vgk", FIX_LIGHT},   // Vegas Golden Knights
    {"/nhl/", "wpg", FIX_LIGHT},   // Winnipeg Jets
    {"/nhl/", "wsh", FIX_LIGHT},   // Washington Capitals
    // NBA
    {"/nba/", "atl", FIX_LIGHT},   // Atlanta Hawks
    {"/nba/", "bkn", FIX_LIGHT},   // Brooklyn Nets
    {"/nba/", "bos", FIX_LIGHT},   // Boston Celtics
    {"/nba/", "cha", FIX_LIGHT},   // Charlotte Hornets
    {"/nba/", "chi", FIX_LIGHT},   // Chicago Bulls
    {"/nba/", "cle", FIX_LIGHT},   // Cleveland Cavaliers
    {"/nba/", "dal", FIX_LIGHT},   // Dallas Mavericks
    {"/nba/", "den", FIX_LIGHT},   // Denver Nuggets
    {"/nba/", "det", FIX_LIGHT},   // Detroit Pistons
    {"/nba/", "gs", FIX_LIGHT},   // Golden State Warriors
    {"/nba/", "hou", FIX_LIGHT},   // Houston Rockets
    {"/nba/", "ind", FIX_LIGHT},   // Indiana Pacers
    {"/nba/", "lac", FIX_LIGHT},   // LA Clippers
    {"/nba/", "lal", FIX_LIGHT},   // Los Angeles Lakers
    {"/nba/", "mem", FIX_LIGHT},   // Memphis Grizzlies
    {"/nba/", "mia", FIX_LIGHT},   // Miami Heat
    {"/nba/", "mil", FIX_LIGHT},   // Milwaukee Bucks
    {"/nba/", "min", FIX_LIGHT},   // Minnesota Timberwolves
    {"/nba/", "no", FIX_LIGHT},   // New Orleans Pelicans
    {"/nba/", "ny", FIX_LIGHT},   // New York Knicks
    {"/nba/", "okc", FIX_LIGHT},   // Oklahoma City Thunder
    {"/nba/", "orl", FIX_LIGHT},   // Orlando Magic
    {"/nba/", "phi", FIX_LIGHT | FIX_CROP_TOP},   // Philadelphia 76ers
    {"/nba/", "phx", FIX_KEYLINE},   // Phoenix Suns
    {"/nba/", "por", FIX_LIGHT},   // Portland Trail Blazers
    {"/nba/", "sa", FIX_LIGHT},   // San Antonio Spurs
    {"/nba/", "sac", FIX_LIGHT},   // Sacramento Kings
    {"/nba/", "tor", FIX_LIGHT},   // Toronto Raptors
    {"/nba/", "utah", FIX_LIGHT},   // Utah Jazz
    {"/nba/", "wsh", FIX_LIGHT},   // Washington Wizards
    // college (ESPN team id)
    {"/ncaa/", "333", FIX_LIGHT},   // Alabama
    {"/ncaa/", "8", FIX_LIGHT},   // Arkansas
    {"/ncaa/", "2", FIX_LIGHT},   // Auburn
    {"/ncaa/", "239", FIX_LIGHT},   // Baylor
    {"/ncaa/", "103", FIX_KEYLINE},   // Boston College
    {"/ncaa/", "252", FIX_LIGHT},   // BYU
    {"/ncaa/", "25", FIX_KEYLINE},   // California
    {"/ncaa/", "228", FIX_LIGHT},   // Clemson
    {"/ncaa/", "38", FIX_LIGHT},   // Colorado
    {"/ncaa/", "150", FIX_LIGHT},   // Duke
    {"/ncaa/", "57", FIX_LIGHT},   // Florida
    {"/ncaa/", "59", FIX_KEYLINE},   // Georgia Tech
    {"/ncaa/", "248", FIX_LIGHT},   // Houston
    {"/ncaa/", "356", FIX_LIGHT},   // Illinois
    {"/ncaa/", "2294", FIX_KEYLINE},   // Iowa
    {"/ncaa/", "84", FIX_LIGHT},   // Indiana
    {"/ncaa/", "2306", FIX_LIGHT},   // Kansas State
    {"/ncaa/", "97", FIX_KEYLINE},   // Louisville
    {"/ncaa/", "99", FIX_KEYLINE},   // LSU
    {"/ncaa/", "2390", FIX_KEYLINE},   // Miami
    {"/ncaa/", "135", FIX_LIGHT},   // Minnesota
    {"/ncaa/", "145", FIX_LIGHT},   // Ole Miss
    {"/ncaa/", "142", FIX_LIGHT},   // Missouri
    {"/ncaa/", "344", FIX_LETTERS},   // Mississippi State
    {"/ncaa/", "127", FIX_LIGHT},   // Michigan State
    {"/ncaa/", "87", FIX_LIGHT},   // Notre Dame
    {"/ncaa/", "158", FIX_KEYLINE},   // Nebraska
    {"/ncaa/", "77", FIX_KEYLINE},   // Northwestern
    {"/ncaa/", "197", FIX_LIGHT},   // Oklahoma State
    {"/ncaa/", "2483", FIX_KEYLINE},   // Oregon
    {"/ncaa/", "201", FIX_LIGHT},   // Oklahoma
    {"/ncaa/", "221", FIX_LIGHT},   // Pittsburgh
    {"/ncaa/", "164", FIX_KEYLINE},   // Rutgers
    {"/ncaa/", "2579", FIX_LIGHT},   // South Carolina
    {"/ncaa/", "2567", FIX_LIGHT},   // SMU
    {"/ncaa/", "24", FIX_LIGHT},   // Stanford
    {"/ncaa/", "183", FIX_KEYLINE},   // Syracuse
    {"/ncaa/", "245", FIX_KEYLINE},   // Texas A&M
    {"/ncaa/", "2628", FIX_KEYLINE},   // TCU
    {"/ncaa/", "2633", FIX_LIGHT},   // Tennessee
    {"/ncaa/", "251", FIX_LIGHT},   // Texas
    {"/ncaa/", "26", FIX_KEYLINE},   // UCLA
    {"/ncaa/", "61", FIX_LIGHT},   // Georgia
    {"/ncaa/", "96", FIX_KEYLINE},   // Kentucky
    {"/ncaa/", "153", FIX_LIGHT},   // North Carolina
    {"/ncaa/", "30", FIX_KEYLINE},   // USC
    {"/ncaa/", "254", FIX_KEYLINE},   // Utah
    {"/ncaa/", "238", FIX_LIGHT},   // Vanderbilt
    {"/ncaa/", "154", FIX_KEYLINE},   // Wake Forest
    {"/ncaa/", "264", FIX_LIGHT},   // Washington
    {"/ncaa/", "277", FIX_LIGHT},   // West Virginia
};

// The fix for an ESPN logo address, 0 if none
static inline uint8_t logoFix(const char* url) {
  if (!url) return 0;
  const char* f = strrchr(url, '/');
  if (!f) return 0;
  f++;
  const char* dot = strchr(f, '.');
  size_t n = dot ? (size_t)(dot - f) : strlen(f);
  for (const LogoFix& e : LOGO_FIXES)
    if (strstr(url, e.league) && strlen(e.file) == n && !strncasecmp(f, e.file, n)) return e.flags;
  return 0;
}
