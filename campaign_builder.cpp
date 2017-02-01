#include "stdafx.h"
#include "campaign_builder.h"
#include "options.h"
#include "campaign_type.h"
#include "player_role.h"
#include "view.h"
#include "enemy_factory.h"
#include "villain_type.h"
#include "creature.h"
#include "creature_name.h"
#include "retired_games.h"
#include "view_object.h"
#include "name_generator.h"


optional<Vec2> CampaignBuilder::considerStaticPlayerPos(const Campaign& campaign) {
  switch (campaign.type) {
    case CampaignType::CAMPAIGN:
      return Vec2(1, campaign.sites.getBounds().middle().y);
    case CampaignType::QUICK_MAP:
    case CampaignType::SINGLE_KEEPER:
      return campaign.sites.getBounds().middle();
    default:
      return none;
  }
}

static void setCountLimits(Options* options) {
#ifdef RELEASE
  options->setLimits(OptionId::MAIN_VILLAINS, 1, 4);
#else
  options->setLimits(OptionId::MAIN_VILLAINS, 0, 4);
#endif
  options->setLimits(OptionId::LESSER_VILLAINS, 0, 6);
  options->setLimits(OptionId::ALLIES, 0, 4);
  options->setLimits(OptionId::INFLUENCE_SIZE, 3, 6);
}

vector<OptionId> CampaignBuilder::getSecondaryOptions(CampaignType type) const {
  switch (type) {
    case CampaignType::QUICK_MAP:
    case CampaignType::CAMPAIGN:
      return {};
    case CampaignType::ENDLESS:
      return {OptionId::LESSER_VILLAINS, OptionId::ALLIES};
    case CampaignType::FREE_PLAY:
      return {OptionId::MAIN_VILLAINS, OptionId::LESSER_VILLAINS, OptionId::ALLIES};
    case CampaignType::SINGLE_KEEPER:
      return {};
  }
}

vector<OptionId> CampaignBuilder::getPrimaryOptions() const {
  switch (playerRole) {
    case PlayerRole::KEEPER:
      return {OptionId::KEEPER_NAME, OptionId::KEEPER_TYPE};
    case PlayerRole::ADVENTURER:
      return {OptionId::ADVENTURER_NAME, OptionId::ADVENTURER_TYPE};
  }
}

vector<CampaignType> CampaignBuilder::getAvailableTypes() const {
  switch (playerRole) {
    case PlayerRole::KEEPER:
      return {
        CampaignType::CAMPAIGN,
        CampaignType::ENDLESS,
        CampaignType::FREE_PLAY,
        CampaignType::SINGLE_KEEPER,
#ifndef RELEASE
        CampaignType::QUICK_MAP,
#endif
      };
    case PlayerRole::ADVENTURER:
      return {
        CampaignType::CAMPAIGN,
        CampaignType::FREE_PLAY,
      };
  }
}

optional<string> CampaignBuilder::getSiteChoiceTitle(CampaignType type) const {
  switch (type) {
    case CampaignType::FREE_PLAY:
    case CampaignType::ENDLESS:
      switch (playerRole) {
        case PlayerRole::KEEPER: return "Choose the location of your base:"_s;
        case PlayerRole::ADVENTURER: return "Choose a location to start your adventure:"_s;
      }
      break;
    default:
      return none;
  }
}

vector<Campaign::VillainInfo> CampaignBuilder::getMainVillains() {
  switch (playerRole) {
    case PlayerRole::KEEPER:
      return {
        {ViewId::DUKE, EnemyId::KNIGHTS, "Knights", VillainType::MAIN},
        {ViewId::ELF_LORD, EnemyId::ELVES, "Elves", VillainType::MAIN},
        {ViewId::DWARF_BARON, EnemyId::DWARVES, "Dwarves", VillainType::MAIN},
        {ViewId::RED_DRAGON, EnemyId::RED_DRAGON, "Red dragon", VillainType::MAIN},
        {ViewId::ELEMENTALIST, EnemyId::ELEMENTALIST, "Elementalist", VillainType::MAIN},
        {ViewId::GREEN_DRAGON, EnemyId::GREEN_DRAGON, "Green dragon", VillainType::MAIN},
        {ViewId::LIZARDLORD, EnemyId::LIZARDMEN, "Lizardmen", VillainType::MAIN},
        {ViewId::SHAMAN, EnemyId::WARRIORS, "Warriors", VillainType::MAIN},
      };
    case PlayerRole::ADVENTURER:
      return {
        {ViewId::RED_DRAGON, EnemyId::RED_DRAGON, "Red dragon", VillainType::MAIN},
        {ViewId::GREEN_DRAGON, EnemyId::GREEN_DRAGON, "Green dragon", VillainType::MAIN},
        {ViewId::SHELOB, EnemyId::SHELOB, "Giant spider", VillainType::MAIN},
        {ViewId::ANT_QUEEN, EnemyId::ANTS_OPEN, "Ants", VillainType::MAIN},
        {ViewId::DARK_ELF_LORD, EnemyId::DARK_ELVES, "Dark elves", VillainType::MAIN},
        {ViewId::ORC_CAPTAIN, EnemyId::ORC_VILLAGE, "Greenskin village", VillainType::MAIN},
      };
  }
}

vector<Campaign::VillainInfo> CampaignBuilder::getLesserVillains() {
  switch (playerRole) {
    case PlayerRole::KEEPER:
      return {
        {ViewId::ENT, EnemyId::ENTS, "Tree spirits", VillainType::LESSER},
        {ViewId::DRIAD, EnemyId::DRIADS, "Driads", VillainType::LESSER},
        {ViewId::CYCLOPS, EnemyId::CYCLOPS, "Cyclops", VillainType::LESSER},
        {ViewId::SHELOB, EnemyId::SHELOB, "Giant spider", VillainType::LESSER},
        {ViewId::HYDRA, EnemyId::HYDRA, "Hydra", VillainType::LESSER},
        {ViewId::ANT_QUEEN, EnemyId::ANTS_OPEN, "Ants", VillainType::LESSER},
        {ViewId::ZOMBIE, EnemyId::CEMETERY, "Zombies", VillainType::LESSER},
      };
    case PlayerRole::ADVENTURER:
      return {
        {ViewId::BANDIT, EnemyId::BANDITS, "Bandits", VillainType::LESSER},
        {ViewId::CYCLOPS, EnemyId::CYCLOPS, "Cyclops", VillainType::LESSER},
        {ViewId::HYDRA, EnemyId::HYDRA, "Hydra", VillainType::LESSER},
        {ViewId::ZOMBIE, EnemyId::CEMETERY, "Zombies", VillainType::LESSER},
        {ViewId::OGRE, EnemyId::OGRE_CAVE, "Ogres", VillainType::LESSER},
        {ViewId::HARPY, EnemyId::HARPY_CAVE, "Harpies", VillainType::LESSER},
      };
  }
}

vector<Campaign::VillainInfo> CampaignBuilder::getAllies() {
  switch (playerRole) {
    case PlayerRole::KEEPER:
      return {
        {ViewId::UNKNOWN_MONSTER, EnemyId::OGRE_CAVE, "Unknown", VillainType::ALLY},
        {ViewId::UNKNOWN_MONSTER, EnemyId::HARPY_CAVE, "Unknown", VillainType::ALLY},
        {ViewId::UNKNOWN_MONSTER, EnemyId::SOKOBAN, "Unknown", VillainType::ALLY},
        {ViewId::DARK_ELF_LORD, EnemyId::DARK_ELVES, "Dark elves", VillainType::ALLY},
        {ViewId::GNOME_BOSS, EnemyId::GNOMES, "Gnomes", VillainType::ALLY},
        {ViewId::ORC_CAPTAIN, EnemyId::ORC_VILLAGE, "Greenskin village", VillainType::ALLY},
      };
    case PlayerRole::ADVENTURER:
      return {
        {ViewId::DUKE, EnemyId::KNIGHTS, "Knights", VillainType::ALLY},
        {ViewId::ELF_LORD, EnemyId::ELVES, "Elves", VillainType::ALLY},
        {ViewId::DWARF_BARON, EnemyId::DWARVES, "Dwarves", VillainType::ALLY},
        {ViewId::LIZARDLORD, EnemyId::LIZARDMEN, "Lizardmen", VillainType::ALLY},
      };
  }
}

const char* CampaignBuilder::getIntroText() const {
   switch (playerRole) {
    case PlayerRole::KEEPER:
      return
        "Welcome to the campaign mode! "
        "The world, which you see below, is made up of smaller maps. Pick one, and build your base there. "
        "There are hostile and friendly tribes around you. You have to conquer all villains marked as \"main\" "
        "to win the game. Make sure you add a few retired dungeons created by other players."
        "You can travel to other sites by creating a team and using the travel command.\n\n"
        "The highlighted tribes are in your influence zone, which means that you can currently interact with them "
        "(trade, recruit, attack or be attacked). "
        "As you conquer more enemies, your influence zone grows.\n\n";
    case PlayerRole::ADVENTURER:
      return
        "Welcome to the campaign mode! "
        "The world, which you see below, is made up of smaller maps. Pick one to start your adventure there. "
        "There are hostile and friendly tribes around you. You have to conquer all villains marked as \"main\" "
        "to win the game. Make sure you add a few retired dungeons created by other players."
        "You can travel to other sites by using the travel command.\n\n"
        "The highlighted tribes are in your influence zone, which means that you can currently travel there. "
        "As you conquer more enemies, your influence zone grows.\n\n";
   }
}


bool CampaignBuilder::isStaticPlayerPos(const Campaign& campaign) {
  switch (campaign.type) {
    case CampaignType::QUICK_MAP:
    case CampaignType::CAMPAIGN:
    case CampaignType::SINGLE_KEEPER:
      return true;
    default:
      return false;
  }
}

void CampaignBuilder::setPlayerPos(Campaign& campaign, Vec2 pos, const Creature* player) {
  switch (playerRole) {
    case PlayerRole::KEEPER:
      if (campaign.playerPos)
        campaign.clearSite(*campaign.playerPos);
      campaign.playerPos = pos;
      campaign.sites[*campaign.playerPos].dweller = Campaign::KeeperInfo{player->getViewObject().id()};
      break;
    case PlayerRole:: ADVENTURER:
      campaign.playerPos = pos;
      break;
  }

}

PCreature CampaignBuilder::getPlayerCreature() {
  PCreature ret;
  switch (playerRole) {
    case PlayerRole::KEEPER:
      ret = CreatureFactory::fromId(options->getCreatureId(OptionId::KEEPER_TYPE), TribeId::getKeeper());
      break;
    case PlayerRole::ADVENTURER:
      ret = CreatureFactory::fromId(options->getCreatureId(OptionId::ADVENTURER_TYPE), TribeId::getAdventurer());
      break;
  }
  ret->getName().useFullTitle();
  return ret;
}


static Table<Campaign::SiteInfo> getTerrain(RandomGen& random, Vec2 size, int numBlocked) {
  Table<Campaign::SiteInfo> ret(size, {});
  for (Vec2 v : ret.getBounds())
    ret[v].viewId.push_back(ViewId::GRASS);
  vector<Vec2> freePos = ret.getBounds().getAllSquares();
  for (int i : Range(numBlocked)) {
    Vec2 pos = random.choose(freePos);
    removeElement(freePos, pos);
    ret[pos].setBlocked();
  }
  return ret;
}

struct VillainCounts {
  int numMain;
  int numLesser;
  int numAllies;
};

static VillainCounts getVillainCounts(CampaignType type, Options* options) {
  switch (type) {
    case CampaignType::FREE_PLAY: {
      return {
        options->getIntValue(OptionId::MAIN_VILLAINS),
        options->getIntValue(OptionId::LESSER_VILLAINS),
        options->getIntValue(OptionId::ALLIES)
      };
    }
    case CampaignType::CAMPAIGN:
      return {4, 6, 2};
    case CampaignType::ENDLESS:
      return {
        0,
        options->getIntValue(OptionId::LESSER_VILLAINS),
        options->getIntValue(OptionId::ALLIES)
      };
    case CampaignType::QUICK_MAP:
    case CampaignType::SINGLE_KEEPER:
      return {0, 0, 0};
  }
}

CampaignBuilder::CampaignBuilder(View* v, RandomGen& rand, Options* o, PlayerRole r)
    : view(v), random(rand), playerRole(r), options(o) {
}

static string getNewIdSuffix() {
  vector<char> chars;
  for (char c : Range(128))
    if (isalnum(c))
      chars.push_back(c);
  string ret;
  for (int i : Range(4))
    ret += Random.choose(chars);
  return ret;
}

struct VillainPlacement {
  Range xRange;
  optional<Vec2> firstLocation;
};

void CampaignBuilder::placeVillains(Campaign& campaign, vector<Campaign::SiteInfo::Dweller> villains,
    const VillainPlacement& placement, int count) {
  random.shuffle(villains.begin(), villains.end());
  if (villains.size() > count)
    villains.resize(count);
  vector<Vec2> freePos;
  for (Vec2 v : campaign.sites.getBounds())
    if (!campaign.sites[v].blocked && campaign.sites[v].isEmpty() && placement.xRange.contains(v.x))
      freePos.push_back(v);
  freePos = random.permutation(freePos);
  if (auto& pos = placement.firstLocation)
    freePos = concat({*pos}, freePos);
  for (int i : All(villains))
    campaign.sites[freePos[i]].dweller = villains[i];
}

VillainPlacement CampaignBuilder::getVillainPlacement(const Campaign& campaign, VillainType type) {
  VillainPlacement ret { campaign.sites.getBounds().getXRange(), none };
  int width = campaign.sites.getBounds().right();
  switch (campaign.getType()) {
    case CampaignType::CAMPAIGN:
      switch (type) {
        case VillainType::LESSER:
          ret.xRange = Range(2, 2 * width / 3);
          break;
        case VillainType::MAIN:
          ret.xRange = Range(width / 2, width);
          break;
        case VillainType::ALLY:
          if (campaign.getPlayerRole() == PlayerRole::ADVENTURER)
            ret.firstLocation = *considerStaticPlayerPos(campaign);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  return ret;
}

using Dweller = Campaign::SiteInfo::Dweller;

template <typename T>
vector<Dweller> shuffle(RandomGen& random, vector<T> v) {
  random.shuffle(v.begin(), v.end());
  return transform2<Dweller>(v, [](const T& t) { return Dweller(t); });
}

void CampaignBuilder::placeVillains(Campaign& campaign, const VillainCounts& counts, const optional<RetiredGames>& retired) {
  int numRetired = retired ? min(counts.numMain, retired->getNumActive()) : 0;
  placeVillains(campaign, shuffle(random, getMainVillains()), getVillainPlacement(campaign, VillainType::MAIN),
      counts.numMain - numRetired);
  placeVillains(campaign, shuffle(random, getLesserVillains()), getVillainPlacement(campaign, VillainType::LESSER),
      counts.numLesser);
  placeVillains(campaign, shuffle(random, getAllies()), getVillainPlacement(campaign, VillainType::ALLY),
      counts.numAllies);
  if (retired) {
    placeVillains(campaign, transform2<Dweller>(retired->getActiveGames(),
        [](const RetiredGames::RetiredGame& game) -> Dweller {
          return Campaign::RetiredInfo{game.gameInfo, game.fileInfo};
        }), getVillainPlacement(campaign, VillainType::MAIN), numRetired);
  }
}

optional<CampaignSetup> CampaignBuilder::prepareCampaign(function<RetiredGames()> genRetired, CampaignType type) {
  Vec2 size(16, 9);
  int numBlocked = 0.6 * size.x * size.y;
  Table<Campaign::SiteInfo> terrain = getTerrain(random, size, numBlocked);
  optional<RetiredGames> retiredCache;
  static optional<RetiredGames> noRetired;
  View::CampaignMenuState menuState {};
  setCountLimits(options);
  options->setChoices(OptionId::KEEPER_TYPE, {CreatureId::KEEPER, CreatureId::KEEPER_F});
  options->setChoices(OptionId::ADVENTURER_TYPE, {CreatureId::ADVENTURER, CreatureId::ADVENTURER_F});
  while (1) {
    PCreature player = getPlayerCreature();
    Campaign campaign(terrain, type, playerRole, NameGenerator::get(NameGeneratorId::WORLD)->getNext());
    if (auto pos = considerStaticPlayerPos(campaign)) {
      campaign.clearSite(*pos);
      setPlayerPos(campaign, *pos, player.get());
    }
    if (type == CampaignType::FREE_PLAY && !retiredCache)
      retiredCache = genRetired();
    auto& retired = type == CampaignType::FREE_PLAY ? retiredCache : noRetired;
    placeVillains(campaign, getVillainCounts(type, options), retired);
    while (1) {
      bool updateMap = false;
      campaign.influenceSize = options->getIntValue(OptionId::INFLUENCE_SIZE);
      campaign.refreshInfluencePos();
      CampaignAction action = type == CampaignType::QUICK_MAP ? CampaignActionId::CONFIRM
          : view->prepareCampaign({
              campaign,
              retired,
              player.get(),
              getPrimaryOptions(),
              getSecondaryOptions(type),
              getSiteChoiceTitle(type),
              getIntroText(),
              getAvailableTypes()
              }, options, menuState);
      switch (action.getId()) {
        case CampaignActionId::REROLL_MAP:
            terrain = getTerrain(random, size, numBlocked);
            updateMap = true;
            break;
        case CampaignActionId::UPDATE_MAP:
            updateMap = true;
            break;
        case CampaignActionId::CHANGE_TYPE:
            type = action.get<CampaignType>();
            updateMap = true;
            break;
        case CampaignActionId::UPDATE_OPTION:
            switch (action.get<OptionId>()) {
              case OptionId::KEEPER_TYPE:
              case OptionId::ADVENTURER_TYPE:
                player = getPlayerCreature();
                if (campaign.playerPos) {
                  setPlayerPos(campaign, *campaign.playerPos, player.get());
                }
                break;
              case OptionId::KEEPER_NAME:
              case OptionId::ADVENTURER_NAME:
              case OptionId::INFLUENCE_SIZE: break;
              default: updateMap = true; break;
            }
            break;
        case CampaignActionId::CANCEL:
            return none;
        case CampaignActionId::CHOOSE_SITE:
            if (!isStaticPlayerPos(campaign))
              setPlayerPos(campaign, action.get<Vec2>(), player.get());
            break;
        case CampaignActionId::CONFIRM:
            if (!retired || retired->getNumActive() > 0 || playerRole != PlayerRole::KEEPER ||
                retired->getAllGames().empty() ||
                view->yesOrNoPrompt("The imps are going to be sad if you don't add any retired dungeons. Continue?")) {
              string name = *player->getName().first();
              string gameIdentifier = name + "_" + campaign.worldName + getNewIdSuffix();
              string gameDisplayName = name + " of " + campaign.worldName;
              return CampaignSetup{campaign, std::move(player), gameIdentifier, gameDisplayName};
            }
      }
      if (updateMap)
        break;
    }
  }
}

CampaignSetup CampaignBuilder::getEmptyCampaign() {
  Campaign ret(Table<Campaign::SiteInfo>(1, 1), CampaignType::SINGLE_KEEPER, PlayerRole::KEEPER, "");
  return CampaignSetup{ret, PCreature(nullptr), "", ""};
}