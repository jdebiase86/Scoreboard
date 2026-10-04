// Every team the setup page offers. abbr is ESPN's own abbreviation;
// name is what ESPN calls the team (displayName for the pros, the school
// for college) - used as a fallback match if ESPN ever changes an
// abbreviation. group: ESPN's conference number for college teams, so the
// board can ask for that conference's games (a few dozen KB) instead of
// every FBS game on a Saturday (well over a megabyte).
#pragma once
#include <stdint.h>
#include "sb_types.h"

enum League : uint8_t { L_NFL = 0, L_CFB, L_MLB, L_NHL, L_NBA, L_COUNT };

struct TeamDef { League league; const char* abbr; const char* name; uint8_t group; };

static const char* const LEAGUE_NAMES[L_COUNT] = {"NFL", "College football", "MLB", "NHL", "NBA"};

static inline Sport leagueSport(League l) {
  return l == L_NFL || l == L_CFB ? FOOTBALL : l == L_MLB ? BASEBALL : l == L_NHL ? HOCKEY : BASKETBALL;
}

static const TeamDef TEAMS[] = {
  // NFL
  {L_NFL,"ARI","Arizona Cardinals",0},{L_NFL,"ATL","Atlanta Falcons",0},{L_NFL,"BAL","Baltimore Ravens",0},
  {L_NFL,"BUF","Buffalo Bills",0},{L_NFL,"CAR","Carolina Panthers",0},{L_NFL,"CHI","Chicago Bears",0},
  {L_NFL,"CIN","Cincinnati Bengals",0},{L_NFL,"CLE","Cleveland Browns",0},{L_NFL,"DAL","Dallas Cowboys",0},
  {L_NFL,"DEN","Denver Broncos",0},{L_NFL,"DET","Detroit Lions",0},{L_NFL,"GB","Green Bay Packers",0},
  {L_NFL,"HOU","Houston Texans",0},{L_NFL,"IND","Indianapolis Colts",0},{L_NFL,"JAX","Jacksonville Jaguars",0},
  {L_NFL,"KC","Kansas City Chiefs",0},{L_NFL,"LV","Las Vegas Raiders",0},{L_NFL,"LAC","Los Angeles Chargers",0},
  {L_NFL,"LAR","Los Angeles Rams",0},{L_NFL,"MIA","Miami Dolphins",0},{L_NFL,"MIN","Minnesota Vikings",0},
  {L_NFL,"NE","New England Patriots",0},{L_NFL,"NO","New Orleans Saints",0},{L_NFL,"NYG","New York Giants",0},
  {L_NFL,"NYJ","New York Jets",0},{L_NFL,"PHI","Philadelphia Eagles",0},{L_NFL,"PIT","Pittsburgh Steelers",0},
  {L_NFL,"SF","San Francisco 49ers",0},{L_NFL,"SEA","Seattle Seahawks",0},{L_NFL,"TB","Tampa Bay Buccaneers",0},
  {L_NFL,"TEN","Tennessee Titans",0},{L_NFL,"WSH","Washington Commanders",0},
  // College football - SEC (8)
  {L_CFB,"ALA","Alabama",8},{L_CFB,"ARK","Arkansas",8},{L_CFB,"AUB","Auburn",8},{L_CFB,"FLA","Florida",8},
  {L_CFB,"UGA","Georgia",8},{L_CFB,"UK","Kentucky",8},{L_CFB,"LSU","LSU",8},{L_CFB,"MISS","Ole Miss",8},
  {L_CFB,"MSST","Mississippi State",8},{L_CFB,"MIZ","Missouri",8},{L_CFB,"OU","Oklahoma",8},
  {L_CFB,"SC","South Carolina",8},{L_CFB,"TENN","Tennessee",8},{L_CFB,"TEX","Texas",8},
  {L_CFB,"TA&M","Texas A&M",8},{L_CFB,"VAN","Vanderbilt",8},
  // Big Ten (5)
  {L_CFB,"ILL","Illinois",5},{L_CFB,"IU","Indiana",5},{L_CFB,"IOWA","Iowa",5},{L_CFB,"MD","Maryland",5},
  {L_CFB,"MICH","Michigan",5},{L_CFB,"MSU","Michigan State",5},{L_CFB,"MINN","Minnesota",5},
  {L_CFB,"NEB","Nebraska",5},{L_CFB,"NU","Northwestern",5},{L_CFB,"OSU","Ohio State",5},{L_CFB,"ORE","Oregon",5},
  {L_CFB,"PSU","Penn State",5},{L_CFB,"PUR","Purdue",5},{L_CFB,"RUTG","Rutgers",5},{L_CFB,"UCLA","UCLA",5},
  {L_CFB,"USC","USC",5},{L_CFB,"WASH","Washington",5},{L_CFB,"WIS","Wisconsin",5},
  // ACC (1)
  {L_CFB,"BC","Boston College",1},{L_CFB,"CAL","California",1},{L_CFB,"CLEM","Clemson",1},{L_CFB,"DUKE","Duke",1},
  {L_CFB,"FSU","Florida State",1},{L_CFB,"GT","Georgia Tech",1},{L_CFB,"LOU","Louisville",1},
  {L_CFB,"MIA","Miami",1},{L_CFB,"UNC","North Carolina",1},{L_CFB,"NCSU","NC State",1},{L_CFB,"PITT","Pittsburgh",1},
  {L_CFB,"SMU","SMU",1},{L_CFB,"STAN","Stanford",1},{L_CFB,"SYR","Syracuse",1},{L_CFB,"UVA","Virginia",1},
  {L_CFB,"VT","Virginia Tech",1},{L_CFB,"WAKE","Wake Forest",1},
  // Big 12 (4)
  {L_CFB,"ARIZ","Arizona",4},{L_CFB,"ASU","Arizona State",4},{L_CFB,"BAY","Baylor",4},{L_CFB,"BYU","BYU",4},
  {L_CFB,"UCF","UCF",4},{L_CFB,"CIN","Cincinnati",4},{L_CFB,"COLO","Colorado",4},{L_CFB,"HOU","Houston",4},
  {L_CFB,"ISU","Iowa State",4},{L_CFB,"KU","Kansas",4},{L_CFB,"KSU","Kansas State",4},
  {L_CFB,"OKST","Oklahoma State",4},{L_CFB,"TCU","TCU",4},{L_CFB,"TTU","Texas Tech",4},{L_CFB,"UTAH","Utah",4},
  {L_CFB,"WVU","West Virginia",4},
  // Independents (18)
  {L_CFB,"ND","Notre Dame",18},{L_CFB,"ARMY","Army",0},{L_CFB,"NAVY","Navy",0},
  // MLB
  {L_MLB,"ARI","Arizona Diamondbacks",0},{L_MLB,"ATH","Athletics",0},{L_MLB,"ATL","Atlanta Braves",0},
  {L_MLB,"BAL","Baltimore Orioles",0},{L_MLB,"BOS","Boston Red Sox",0},{L_MLB,"CHC","Chicago Cubs",0},
  {L_MLB,"CHW","Chicago White Sox",0},{L_MLB,"CIN","Cincinnati Reds",0},{L_MLB,"CLE","Cleveland Guardians",0},
  {L_MLB,"COL","Colorado Rockies",0},{L_MLB,"DET","Detroit Tigers",0},{L_MLB,"HOU","Houston Astros",0},
  {L_MLB,"KC","Kansas City Royals",0},{L_MLB,"LAA","Los Angeles Angels",0},{L_MLB,"LAD","Los Angeles Dodgers",0},
  {L_MLB,"MIA","Miami Marlins",0},{L_MLB,"MIL","Milwaukee Brewers",0},{L_MLB,"MIN","Minnesota Twins",0},
  {L_MLB,"NYM","New York Mets",0},{L_MLB,"NYY","New York Yankees",0},{L_MLB,"PHI","Philadelphia Phillies",0},
  {L_MLB,"PIT","Pittsburgh Pirates",0},{L_MLB,"SD","San Diego Padres",0},{L_MLB,"SF","San Francisco Giants",0},
  {L_MLB,"SEA","Seattle Mariners",0},{L_MLB,"STL","St. Louis Cardinals",0},{L_MLB,"TB","Tampa Bay Rays",0},
  {L_MLB,"TEX","Texas Rangers",0},{L_MLB,"TOR","Toronto Blue Jays",0},{L_MLB,"WSH","Washington Nationals",0},
  // NHL
  {L_NHL,"ANA","Anaheim Ducks",0},{L_NHL,"BOS","Boston Bruins",0},{L_NHL,"BUF","Buffalo Sabres",0},
  {L_NHL,"CGY","Calgary Flames",0},{L_NHL,"CAR","Carolina Hurricanes",0},{L_NHL,"CHI","Chicago Blackhawks",0},
  {L_NHL,"COL","Colorado Avalanche",0},{L_NHL,"CBJ","Columbus Blue Jackets",0},{L_NHL,"DAL","Dallas Stars",0},
  {L_NHL,"DET","Detroit Red Wings",0},{L_NHL,"EDM","Edmonton Oilers",0},{L_NHL,"FLA","Florida Panthers",0},
  {L_NHL,"LA","Los Angeles Kings",0},{L_NHL,"MIN","Minnesota Wild",0},{L_NHL,"MTL","Montreal Canadiens",0},
  {L_NHL,"NSH","Nashville Predators",0},{L_NHL,"NJ","New Jersey Devils",0},{L_NHL,"NYI","New York Islanders",0},
  {L_NHL,"NYR","New York Rangers",0},{L_NHL,"OTT","Ottawa Senators",0},{L_NHL,"PHI","Philadelphia Flyers",0},
  {L_NHL,"PIT","Pittsburgh Penguins",0},{L_NHL,"SJ","San Jose Sharks",0},{L_NHL,"SEA","Seattle Kraken",0},
  {L_NHL,"STL","St. Louis Blues",0},{L_NHL,"TB","Tampa Bay Lightning",0},{L_NHL,"TOR","Toronto Maple Leafs",0},
  {L_NHL,"UTAH","Utah Mammoth",0},{L_NHL,"VAN","Vancouver Canucks",0},{L_NHL,"VGK","Vegas Golden Knights",0},
  {L_NHL,"WSH","Washington Capitals",0},{L_NHL,"WPG","Winnipeg Jets",0},
  // NBA
  {L_NBA,"ATL","Atlanta Hawks",0},{L_NBA,"BOS","Boston Celtics",0},{L_NBA,"BKN","Brooklyn Nets",0},
  {L_NBA,"CHA","Charlotte Hornets",0},{L_NBA,"CHI","Chicago Bulls",0},{L_NBA,"CLE","Cleveland Cavaliers",0},
  {L_NBA,"DAL","Dallas Mavericks",0},{L_NBA,"DEN","Denver Nuggets",0},{L_NBA,"DET","Detroit Pistons",0},
  {L_NBA,"GS","Golden State Warriors",0},{L_NBA,"HOU","Houston Rockets",0},{L_NBA,"IND","Indiana Pacers",0},
  {L_NBA,"LAC","LA Clippers",0},{L_NBA,"LAL","Los Angeles Lakers",0},{L_NBA,"MEM","Memphis Grizzlies",0},
  {L_NBA,"MIA","Miami Heat",0},{L_NBA,"MIL","Milwaukee Bucks",0},{L_NBA,"MIN","Minnesota Timberwolves",0},
  {L_NBA,"NO","New Orleans Pelicans",0},{L_NBA,"NY","New York Knicks",0},{L_NBA,"OKC","Oklahoma City Thunder",0},
  {L_NBA,"ORL","Orlando Magic",0},{L_NBA,"PHI","Philadelphia 76ers",0},{L_NBA,"PHX","Phoenix Suns",0},
  {L_NBA,"POR","Portland Trail Blazers",0},{L_NBA,"SAC","Sacramento Kings",0},{L_NBA,"SA","San Antonio Spurs",0},
  {L_NBA,"TOR","Toronto Raptors",0},{L_NBA,"UTAH","Utah Jazz",0},{L_NBA,"WSH","Washington Wizards",0},
};
static const int NTEAMS = sizeof(TEAMS) / sizeof(TEAMS[0]);

// The NHL's and NBA's own feeds (power play, bonus) use their own codes
static inline const char* nhlCode(const char* espn) {
  static const char* const M[][2] = {{"NJ","NJD"},{"LA","LAK"},{"TB","TBL"},{"SJ","SJS"},{"UTAH","UTA"}};
  for (auto& m : M) if (!strcmp(m[0], espn)) return m[1];
  return espn;
}
static inline const char* nbaCode(const char* espn) {
  static const char* const M[][2] = {{"GS","GSW"},{"NY","NYK"},{"NO","NOP"},{"SA","SAS"},{"UTAH","UTA"},{"WSH","WAS"}};
  for (auto& m : M) if (!strcmp(m[0], espn)) return m[1];
  return espn;
}
