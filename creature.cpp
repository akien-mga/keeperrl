/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"

#include "creature.h"
#include "creature_factory.h"
#include "level.h"
#include "ranged_weapon.h"
#include "statistics.h"
#include "options.h"
#include "game.h"
#include "effect.h"
#include "item_factory.h"
#include "controller.h"
#include "player_message.h"
#include "attack.h"
#include "vision.h"
#include "equipment.h"
#include "shortest_path.h"
#include "spell_map.h"
#include "minion_task_map.h"
#include "tribe.h"
#include "creature_attributes.h"
#include "position.h"
#include "view.h"
#include "sound.h"
#include "lasting_effect.h"
#include "attack_type.h"
#include "attack_level.h"
#include "model.h"
#include "view_object.h"
#include "spell.h"
#include "body.h"
#include "field_of_view.h"
#include "furniture.h"
#include "creature_debt.h"
#include "message_generator.h"
#include "weapon_info.h"
#include "time_queue.h"

template <class Archive>
void Creature::serialize(Archive& ar, const unsigned int version) {
  ar & SUBCLASS(OwnedObject<Creature>) & SUBCLASS(Renderable) & SUBCLASS(UniqueEntity);
  ar(attributes, position, equipment, shortestPath, knownHiding, tribe, morale);
  ar(deathTime, hidden, lastMoveCounter);
  ar(deathReason, swapPositionCooldown);
  ar(unknownAttackers, privateEnemies, holding);
  ar(controllerStack, kills, statuses);
  ar(difficultyPoints, points);
  ar(vision, lastCombatIntent, debt, lastDamageType, highestAttackValueEver);
}

SERIALIZABLE(Creature)

SERIALIZATION_CONSTRUCTOR_IMPL(Creature)

Creature::Creature(const ViewObject& object, TribeId t, CreatureAttributes attr)
    : Renderable(object), attributes(std::move(attr)), tribe(t) {
  modViewObject().setCreatureId(getUniqueId());
}

Creature::Creature(TribeId t, CreatureAttributes attr)
    : Creature(attr.createViewObject(), t, std::move(attr)) {
}

Creature::~Creature() {
}

vector<vector<WCreature>> Creature::stack(const vector<WCreature>& creatures) {
  map<string, vector<WCreature>> stacks;
  for (WCreature c : creatures)
    stacks[c->getName().stack()].push_back(c);
  return getValues(stacks);
}

const ViewObject& Creature::getViewObjectFor(const Tribe* observer) const {
  if (attributes->getIllusionViewObject() && observer->isEnemy(this))
    return *attributes->getIllusionViewObject();
  else
    return getViewObject();
}

const Body& Creature::getBody() const {
  return attributes->getBody();
}

Body& Creature::getBody() {
  return attributes->getBody();
}

TimeInterval Creature::getSpellDelay(Spell* spell) const {
  CHECK(!isReady(spell));
  return attributes->getSpellMap().getReadyTime(spell) - *getGlobalTime();
}

bool Creature::isReady(Spell* spell) const {
  if (auto time = getGlobalTime())
    return attributes->getSpellMap().getReadyTime(spell) <= *time;
  else
    return true;
}

static double getWillpowerMult(double sorcerySkill) {
  return 2 * pow(0.25, sorcerySkill);
}

const CreatureAttributes& Creature::getAttributes() const {
  return *attributes;
}

CreatureAttributes& Creature::getAttributes() {
  return *attributes;
}

CreatureAction Creature::castSpell(Spell* spell) const {
  if (!attributes->getSpellMap().contains(spell))
    return CreatureAction("You don't know this spell.");
  CHECK(!spell->isDirected());
  if (!isReady(spell))
    return CreatureAction("You can't cast this spell yet.");
  return CreatureAction(this, [=] (WCreature c) {
    c->addSound(spell->getSound());
    spell->addMessage(c);
    spell->getEffect().applyToCreature(c);
    getGame()->getStatistics().add(StatId::SPELL_CAST);
    c->attributes->getSpellMap().setReadyTime(spell, *getGlobalTime() + TimeInterval(
        int(spell->getDifficulty() * getWillpowerMult(attributes->getSkills().getValue(SkillId::SORCERY)))));
    c->spendTime();
  });
}

CreatureAction Creature::castSpell(Spell* spell, Vec2 dir) const {
  CHECK(attributes->getSpellMap().contains(spell));
  CHECK(spell->isDirected());
  CHECK(dir.length8() == 1);
  if (!isReady(spell))
    return CreatureAction("You can't cast this spell yet.");
  return CreatureAction(this, [=] (WCreature c) {
    c->addSound(spell->getSound());
    thirdPerson(getName().the() + " casts a spell");
    secondPerson("You cast " + spell->getName());
    applyDirected(c, dir, spell->getDirEffectType());
    getGame()->getStatistics().add(StatId::SPELL_CAST);
    c->attributes->getSpellMap().setReadyTime(spell, *getGlobalTime() + TimeInterval(
        int(spell->getDifficulty() * getWillpowerMult(attributes->getSkills().getValue(SkillId::SORCERY)))));
    c->spendTime();
  });
}

void Creature::pushController(PController ctrl) {
  if (auto controller = getController())
    controller->onEndedControl();
  controllerStack.push_back(std::move(ctrl));
  getController()->onStartedControl();
}

void Creature::setController(PController ctrl) {
  if (auto controller = getController())
    controller->onEndedControl();
  controllerStack.clear();
  pushController(std::move(ctrl));
  getController()->onStartedControl();
}

void Creature::popController() {
  if (!controllerStack.empty()) {
    getController()->onEndedControl();
    controllerStack.pop_back();
    if (auto controller = getController())
      controller->onStartedControl();
  }
}

bool Creature::isDead() const {
  return !!deathTime;
}

GlobalTime Creature::getDeathTime() const {
  return *deathTime;
}

void Creature::clearLastAttacker() {
  lastAttacker = nullptr;
}

optional<string> Creature::getDeathReason() const {
  if (deathReason)
    return deathReason;
  if (lastAttacker)
    return "killed by " + lastAttacker->getName().a();
  return none;
}

const EntitySet<Creature>& Creature::getKills() const {
  return kills;
}

int Creature::getLastMoveCounter() const {
  return lastMoveCounter;
}

const EnumSet<CreatureStatus>& Creature::getStatus() const {
  return statuses;
}

EnumSet<CreatureStatus>& Creature::getStatus() {
  return statuses;
}

optional<MovementInfo> Creature::spendTime(TimeInterval t) {
  if (WModel m = position.getModel()) {
    MovementInfo ret(Vec2(0, 0), *getLocalTime(), *getLocalTime() + t, 0, MovementInfo::MOVE);
    lastMoveCounter = ret.moveCounter = position.getModel()->getMoveCounter();
    if (!isDead()) {
      if (isAffected(LastingEffect::SPEED) && t == 1_visible) {
        if (m->getTimeQueue().hasExtraMove(this))
          ret.tBegin += 0.5;
        else
          ret.tEnd -= 0.5;
        m->getTimeQueue().makeExtraMove(this);
      } else {
        if (isAffected(LastingEffect::SPEED))
          t = t - 1_visible;
        if (isAffected(LastingEffect::SLOWED))
          t *= 2;
        m->getTimeQueue().increaseTime(this, t);
      }
    }
    m->increaseMoveCounter();
    hidden = false;
    return ret;
  }
  return none;
}

CreatureAction Creature::forceMove(Vec2 dir) const {
  return forceMove(getPosition().plus(dir));
}

CreatureAction Creature::forceMove(Position pos) const {
  const_cast<Creature*>(this)->forceMovement = true;
  CreatureAction action = move(pos);
  const_cast<Creature*>(this)->forceMovement = false;
  if (action)
    return action.prepend([] (WCreature c) { c->forceMovement = true; })
      .append([] (WCreature c) { c->forceMovement = false; });
  else
    return action;
}

CreatureAction Creature::move(Vec2 dir) const {
  return move(getPosition().plus(dir));
}

CreatureAction Creature::move(Position pos) const {
  Vec2 direction = getPosition().getDir(pos);
  if (getHoldingCreature())
    return CreatureAction("You can't break free!");
  if (direction.length8() != 1)
    return CreatureAction();
  if (!position.canMoveCreature(direction)) {
    if (pos.getCreature()) {
      if (!canSwapPositionInMovement(pos.getCreature()))
        return CreatureAction(/*"You can't swap position with " + pos.getCreature()->getName().the()*/);
    } else
      return CreatureAction();
  }
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " moving " << direction;
    if (isAffected(LastingEffect::ENTANGLED) || isAffected(LastingEffect::TIED_UP)) {
      secondPerson("You can't break free!");
      thirdPerson(getName().the() + " can't break free!");
      self->spendTime();
      return;
    }
    if (position.canMoveCreature(direction))
      self->position.moveCreature(direction);
    else {
      self->swapPosition(direction);
      return;
    }
    auto timeSpent = 1_visible;
    if (isAffected(LastingEffect::COLLAPSED)) {
      you(MsgType::CRAWL, getPosition().getName());
      timeSpent = 3_visible;
    }
    self->addMovementInfo(self->spendTime(timeSpent)->setDirection(direction));
  });
}

void Creature::displace(Vec2 dir) {
  position.moveCreature(dir);
  auto time = *getLocalTime();
  addMovementInfo({dir, time, time + 1_visible, position.getModel()->getMoveCounter(), MovementInfo::MOVE});
}

bool Creature::canTakeItems(const vector<WItem>& items) const {
  return getBody().isHumanoid() && canCarry(items);
}

void Creature::takeItems(vector<PItem> items, WCreature from) {
  vector<WItem> ref = getWeakPointers(items);
  equipment->addItems(std::move(items));
  getController()->onItemsGiven(ref, from);
}

void Creature::you(MsgType type, const string& param) const {
  getController()->getMessageGenerator().add(this, type, param);
}

void Creature::you(const string& param) const {
  getController()->getMessageGenerator().add(this, param);
}

void Creature::verb(const char* second, const char* third, const std::string& param) {
  secondPerson("You "_s + second + " " + param);
  thirdPerson(getName().the() + " " + third + " "_s + param);
}

void Creature::secondPerson(const PlayerMessage& message) const {
  getController()->getMessageGenerator().addSecondPerson(this, message);
}

WController Creature::getController() const {
  if (!controllerStack.empty())
    return controllerStack.back().get();
  else
    return nullptr;
}

bool Creature::hasCondition(CreatureCondition condition) const {
  for (auto effect : LastingEffects::getCausingCondition(condition))
    if (isAffected(effect))
      return true;
  return false;
}

bool Creature::canSwapPositionInMovement(WCreature other) const {
  return !other->hasCondition(CreatureCondition::RESTRICTED_MOVEMENT)
      && (swapPositionCooldown == 0 || isPlayer())
      && !other->getAttributes().isBoulder()
      && (!other->isPlayer() || isPlayer())
      && !other->isEnemy(this)
      && other->getPosition().canEnterEmpty(this)
      && getPosition().canEnterEmpty(other);
}

void Creature::swapPosition(Vec2 direction) {
  CHECK(direction.length8() == 1);
  WCreature other = NOTNULL(getPosition().plus(direction).getCreature());
  swapPositionCooldown = 4;
  privateMessage("Excuse me!");
  other->privateMessage("Excuse me!");
  position.swapCreatures(other);
  auto movementInfo = *spendTime();
  addMovementInfo(movementInfo.setDirection(direction));
  other->addMovementInfo(movementInfo.setDirection(-direction));
}

void Creature::makeMove() {
  vision->update(this);
  CHECK(!isDead());
  if (hasCondition(CreatureCondition::SLEEPING)) {
    getController()->sleeping();
    spendTime();
    return;
  }
  updateVisibleCreatures();
  updateViewObject();
  if (swapPositionCooldown)
    --swapPositionCooldown;
  {
    // Calls makeMove() while preventing Controller destruction by holding a shared_ptr on stack.
    // This is needed, otherwise Controller could be destroyed during makeMove() if creature committed suicide.
    shared_ptr<Controller> controllerTmp = controllerStack.back().giveMeSharedPointer();
    MEASURE(controllerTmp->makeMove(), "creature move time");
  }

  INFO << getName().bare() << " morale " << getMorale();
  if (!hidden)
    modViewObject().removeModifier(ViewObject::Modifier::HIDDEN);
  unknownAttackers.clear();
  getBody().affectPosition(position);
  highestAttackValueEver = max(highestAttackValueEver, getBestAttack().value);
  vision->update(this);
}

CreatureAction Creature::wait() const {
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " waiting";
    bool keepHiding = hidden;
    self->spendTime();
    self->hidden = keepHiding;
  });
}

const Equipment& Creature::getEquipment() const {
  return *equipment;
}

Equipment& Creature::getEquipment() {
  return *equipment;
}

vector<PItem> Creature::steal(const vector<WItem> items) {
  return equipment->removeItems(items, this);
}

WLevel Creature::getLevel() const {
  return getPosition().getLevel();
}

WGame Creature::getGame() const {
  return getPosition().getGame();
}

Position Creature::getPosition() const {
  return position;
}

void Creature::message(const PlayerMessage& msg) const {
  if (isPlayer())
    getController()->privateMessage(msg);
  else
    getPosition().globalMessage(msg);
}

void Creature::privateMessage(const PlayerMessage& msg) const {
  getController()->privateMessage(msg);
}

void Creature::thirdPerson(const PlayerMessage& playerCanSee) const {
  getController()->getMessageGenerator().addThirdPerson(this, playerCanSee);
}

void Creature::addSkill(Skill* skill) {
  if (!attributes->getSkills().hasDiscrete(skill->getId())) {
    attributes->getSkills().insert(skill->getId());
    privateMessage(skill->getHelpText());
  }
}

vector<WItem> Creature::getPickUpOptions() const {
  if (!getBody().isHumanoid())
    return vector<WItem>();
  else
    return getPosition().getItems();
}

string Creature::getPluralTheName(WItem item, int num) const {
  if (num == 1)
    return item->getTheName(false, this);
  else
    return toString(num) + " " + item->getTheName(true, this);
}

string Creature::getPluralAName(WItem item, int num) const {
  if (num == 1)
    return item->getAName(false, this);
  else
    return toString(num) + " " + item->getAName(true, this);
}

bool Creature::canCarry(const vector<WItem>& items) const {
  if (auto& limit = getBody().getCarryLimit()) {
    double weight = equipment->getTotalWeight();
    for (WItem it : items)
      weight += it->getWeight();
    return weight <= *limit;
  } else
    return true;
}

CreatureAction Creature::pickUp(const vector<WItem>& items) const {
  if (!getBody().isHumanoid())
    return CreatureAction("You can't pick up anything!");
  if (!canCarry(items))
    return CreatureAction("You are carrying too much to pick this up.");
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " pickup ";
    for (auto stack : stackItems(items)) {
      thirdPerson(getName().the() + " picks up " + getPluralAName(stack[0], stack.size()));
      secondPerson("You pick up " + getPluralTheName(stack[0], stack.size()));
    }
    self->equipment->addItems(self->getPosition().removeItems(items));
    if (auto& limit = getBody().getCarryLimit())
      if (equipment->getTotalWeight() > *limit / 2)
        you(MsgType::ARE, "overloaded");
    getGame()->addEvent(EventInfo::ItemsPickedUp{self, items});
    self->spendTime();
  });
}

vector<vector<WItem>> Creature::stackItems(vector<WItem> items) const {
  map<string, vector<WItem> > stacks = groupBy<WItem, string>(items,
      [this] (WItem const& item) { return item->getNameAndModifiers(false, this); });
  return getValues(stacks);
}

CreatureAction Creature::drop(const vector<WItem>& items) const {
  if (!getBody().isHumanoid())
    return CreatureAction("You can't drop this item!");
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " drop";
    for (auto stack : stackItems(items)) {
      thirdPerson(getName().the() + " drops " + getPluralAName(stack[0], stack.size()));
      secondPerson("You drop " + getPluralTheName(stack[0], stack.size()));
    }
    getGame()->addEvent(EventInfo::ItemsDropped{self, items});
    self->getPosition().dropItems(self->equipment->removeItems(items, self));
    self->spendTime();
  });
}

void Creature::drop(vector<PItem> items) {
  getPosition().dropItems(std::move(items));
}

bool Creature::canEquipIfEmptySlot(WConstItem item, string* reason) const {
  if (!getBody().isHumanoid()) {
    if (reason)
      *reason = "Only humanoids can equip items!";
    return false;
  }
  if (!attributes->canEquip()) {
    if (reason)
      *reason = "You can't equip items!";
    return false;
  }
  if (getBody().numGood(BodyPart::ARM) == 0) {
    if (reason)
      *reason = "You have no healthy arms!";
    return false;
  }
  if (getBody().numGood(BodyPart::ARM) == 1 && item->getWeaponInfo().twoHanded) {
    if (reason)
      *reason = "You need two hands to wield " + item->getAName() + "!";
    return false;
  }
  return item->canEquip();
}

bool Creature::canEquip(WConstItem item) const {
  return canEquipIfEmptySlot(item, nullptr) && equipment->canEquip(item);
}

CreatureAction Creature::equip(WItem item) const {
  string reason;
  if (!canEquipIfEmptySlot(item, &reason))
    return CreatureAction(reason);
  if (equipment->getSlotItems(item->getEquipmentSlot()).contains(item))
    return CreatureAction();
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " equip " << item->getName();
    EquipmentSlot slot = item->getEquipmentSlot();
    if (self->equipment->getSlotItems(slot).size() >= self->equipment->getMaxItems(slot)) {
      WItem previousItem = self->equipment->getSlotItems(slot)[0];
      self->equipment->unequip(previousItem, self);
    }
    secondPerson("You equip " + item->getTheName(false, self));
    thirdPerson(getName().the() + " equips " + item->getAName());
    self->equipment->equip(item, slot, self);
    if (WGame game = getGame())
      game->addEvent(EventInfo::ItemsEquipped{self, {item}});
    self->spendTime();
  });
}

CreatureAction Creature::unequip(WItem item) const {
  if (!equipment->isEquipped(item))
    return CreatureAction("This item is not equipped.");
  if (!getBody().isHumanoid())
    return CreatureAction("You can't remove this item!");
  if (getBody().numGood(BodyPart::ARM) == 0)
    return CreatureAction("You have no healthy arms!");
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " unequip";
    CHECK(equipment->isEquipped(item)) << "Item not equipped.";
    EquipmentSlot slot = item->getEquipmentSlot();
    secondPerson("You " + string(slot == EquipmentSlot::WEAPON ? " sheathe " : " remove ") +
        item->getTheName(false, this));
    thirdPerson(getName().the() + (slot == EquipmentSlot::WEAPON ? " sheathes " : " removes ") +
        item->getAName());
    self->equipment->unequip(item, self);
    self->spendTime();
  });
}

CreatureAction Creature::bumpInto(Vec2 direction) const {
  if (WConstCreature other = getPosition().plus(direction).getCreature())
    return CreatureAction(this, [=](WCreature self) {
      other->getController()->onBump(self);
    });
  else
    return CreatureAction();
}

CreatureAction Creature::applySquare(Position pos) const {
  CHECK(pos.dist8(getPosition()) <= 1);
  if (auto furniture = pos.getFurniture(FurnitureLayer::MIDDLE))
    if (furniture->canUse(this))
      return CreatureAction(this, [=](WCreature self) {
        INFO << getName().the() << " applying " << getPosition().getName();
        auto originalPos = getPosition();
        auto usageTime = furniture->getUsageTime();
        furniture->use(pos, self);
        auto movementInfo = *self->spendTime(usageTime);
        if (pos != getPosition() && getPosition() == originalPos)
          self->addMovementInfo(movementInfo
              .setDirection(getPosition().getDir(pos))
              .setMaxLength(1_visible)
              .setType(MovementInfo::ATTACK));
      });
  return CreatureAction();
}

CreatureAction Creature::hide() const {
  if (!attributes->getSkills().hasDiscrete(SkillId::AMBUSH))
    return CreatureAction("You don't have this skill.");
  if (auto furniture = getPosition().getFurniture(FurnitureLayer::MIDDLE))
    if (furniture->canHide())
      return CreatureAction(this, [=](WCreature self) {
        secondPerson("You hide behind the " + furniture->getName());
        thirdPerson(getName().the() + " hides behind the " + furniture->getName());
        self->knownHiding.clear();
        self->modViewObject().setModifier(ViewObject::Modifier::HIDDEN);
        for (WCreature other : getLevel()->getAllCreatures())
          if (other->canSee(this) && other->isEnemy(this)) {
            self->knownHiding.insert(other);
            if (!isAffected(LastingEffect::BLIND))
              you(MsgType::CAN_SEE_HIDING, other->getName().the());
          }
        self->spendTime();
        self->hidden = true;
      });
  return CreatureAction("You can't hide here.");
}

CreatureAction Creature::chatTo(WCreature other) const {
  CHECK(other);
  if (other->getPosition().dist8(getPosition()) == 1)
    return CreatureAction(this, [=](WCreature self) {
        secondPerson("You chat with " + other->getName().the());
        thirdPerson(getName().the() + " chats with " + other->getName().the());
        other->getAttributes().chatReaction(other, self);
        self->spendTime();
    });
  else
    return CreatureAction("Move closer to chat to " + other->getName().the());
}

CreatureAction Creature::stealFrom(Vec2 direction, const vector<WItem>& items) const {
  if (getPosition().plus(direction).getCreature())
    return CreatureAction(this, [=](WCreature self) {
        WCreature other = NOTNULL(getPosition().plus(direction).getCreature());
        self->equipment->addItems(other->steal(items));
      });
  return CreatureAction();
}

bool Creature::isHidden() const {
  return hidden;
}

bool Creature::knowsHiding(WConstCreature c) const {
  return knownHiding.contains(c);
}

void Creature::addEffect(LastingEffect effect, TimeInterval time, bool msg) {
  if (LastingEffects::affects(this, effect) && !getBody().isImmuneTo(effect)) {
    bool was = isAffected(effect);
    attributes->addLastingEffect(effect, *getGlobalTime() + time);
    if (!was && isAffected(effect))
      LastingEffects::onAffected(this, effect, msg);
  }
}

void Creature::removeEffect(LastingEffect effect, bool msg) {
  bool was = isAffected(effect);
  attributes->clearLastingEffect(effect, *getGlobalTime());
  if (was && !isAffected(effect))
    LastingEffects::onRemoved(this, effect, msg);
}

void Creature::addPermanentEffect(LastingEffect effect, int count) {
  bool was = isAffected(effect);
  attributes->addPermanentEffect(effect, count);
  if (!was && isAffected(effect))
    LastingEffects::onAffected(this, effect, true);
}

void Creature::removePermanentEffect(LastingEffect effect, int count) {
  bool was = isAffected(effect);
  attributes->removePermanentEffect(effect, count);
  if (was && !isAffected(effect))
    LastingEffects::onRemoved(this, effect, true);
}

bool Creature::isAffected(LastingEffect effect) const {
  if (auto time = getGlobalTime())
    return attributes->isAffected(effect, *time);
  else
    return false;
}

optional<TimeInterval> Creature::getTimeRemaining(LastingEffect effect) const {
  auto t = attributes->getTimeOut(effect);
  if (auto global = getGlobalTime())
    if (t >= *global)
      return t - *global;
  return none;
}

bool Creature::isDarknessSource() const {
  return isAffected(LastingEffect::DARKNESS_SOURCE);
}

// penalty to strength and dexterity per extra attacker in a single turn
int simulAttackPen(int attackers) {
  return max(0, (attackers - 1) * 2);
}

int Creature::getAttr(AttrType type) const {
  double def = getBody().modifyAttr(type, attributes->getRawAttr(type));
  for (WItem item : equipment->getAllEquipped())
    if (item->getClass() != ItemClass::WEAPON || type != item->getWeaponInfo().meleeAttackAttr)
      def += item->getModifier(type);
  LastingEffects::modifyAttr(this, type, def);
  return max(0, (int) def);
}

int Creature::getPoints() const {
  return points;
}

void Creature::onKilled(WCreature victim, optional<ExperienceType> lastDamage) {
  double attackDiff = victim->highestAttackValueEver - highestAttackValueEver;
  constexpr double maxLevelGain = 1.0;
  constexpr double minLevelGain = 0.02;
  constexpr double equalLevelGain = 0.2;
  constexpr double maxLevelDiff = 10;
  double expIncrease = max(minLevelGain, min(maxLevelGain,
      (maxLevelGain - equalLevelGain) * attackDiff / maxLevelDiff + equalLevelGain));
  increaseExpLevel(lastDamage.value_or(ExperienceType::MELEE), expIncrease);
  int difficulty = victim->getDifficultyPoints();
  CHECK(difficulty >=0 && difficulty < 100000) << difficulty << " " << victim->getName().bare();
  points += difficulty;
  kills.insert(victim);
}

Tribe* Creature::getTribe() {
  return getGame()->getTribe(tribe);
}

const Tribe* Creature::getTribe() const {
  return getGame()->getTribe(tribe);
}

TribeId Creature::getTribeId() const {
  return tribe;
}

void Creature::setTribe(TribeId t) {
  tribe = t;
}

bool Creature::isFriend(WConstCreature c) const {
  return !isEnemy(c);
}

bool Creature::isEnemy(WConstCreature c) const {
  if (c == this)
    return false;
  auto result = getTribe()->isEnemy(c) || c->getTribe()->isEnemy(this) ||
    privateEnemies.contains(c) || c->privateEnemies.contains(this);
  return LastingEffects::modifyIsEnemyResult(this, c, result);
}

vector<WItem> Creature::getGold(int num) const {
  vector<WItem> ret;
  for (WItem item : equipment->getItems([](WConstItem it) { return it->getClass() == ItemClass::GOLD; })) {
    ret.push_back(item);
    if (ret.size() == num)
      return ret;
  }
  return ret;
}

void Creature::setPosition(Position pos) {
  if (!pos.isSameLevel(position)) {
    modViewObject().clearMovementInfo();
  }
  if (shortestPath && shortestPath->getLevel() != pos.getLevel())
    shortestPath.reset();
  position = pos;
}

optional<LocalTime> Creature::getLocalTime() const {
  if (WModel m = position.getModel())
    return m->getLocalTime();
  else
    return none;
}

optional<GlobalTime> Creature::getGlobalTime() const {
  if (WGame g = getGame())
    return g->getGlobalTime();
  else
    return none;
}

void Creature::tick() {
  vision->update(this);
  if (Random.roll(5))
    getDifficultyPoints();
  vector<WItem> discarded;
  for (auto item : equipment->getItems()) {
    item->tick(position);
    if (item->isDiscarded())
      discarded.push_back(item);
  }
  for (auto item : discarded)
    equipment->removeItem(item, this);
  for (LastingEffect effect : ENUM_ALL(LastingEffect)) {
    if (attributes->considerTimeout(effect, *getGlobalTime()))
      LastingEffects::onTimedOut(this, effect, true);
    if (isAffected(effect) && LastingEffects::tick(this, effect))
      return;
  }
  updateViewObject();
  if (getBody().tick(this)) {
    dieWithAttacker(lastAttacker);
    return;
  }
}

void Creature::dropWeapon() {
  if (auto weapon = getWeapon())
    if (equipment->hasItem(weapon)) {
      you(MsgType::DROP_WEAPON, weapon->getName());
      getPosition().dropItem(equipment->removeItem(weapon, this));
    }
}

CreatureAction Creature::execute(WCreature c) const {
  if (c->getPosition().dist8(getPosition()) > 1)
    return CreatureAction();
  return CreatureAction(this, [=] (WCreature self) {
    self->secondPerson("You execute " + c->getName().the());
    self->thirdPerson(self->getName().the() + " executes " + c->getName().the());
    c->dieWithAttacker(self);
  });
}

CreatureAction Creature::attack(WCreature other, optional<AttackParams> attackParams) const {
  CHECK(!other->isDead());
  if (!position.isSameLevel(other->getPosition()))
    return CreatureAction();
  Vec2 dir = getPosition().getDir(other->getPosition());
  if (dir.length8() != 1)
    return CreatureAction();
  auto weapon = getWeapon();
  if (attackParams && attackParams->weapon)
    weapon = attackParams->weapon;
  if (!weapon)
    return CreatureAction("No available weapon or intrinsic attack");
  return CreatureAction(this, [=] (WCreature self) {
    INFO << getName().the() << " attacking " << other->getName().the();
    auto damageAttr = weapon->getWeaponInfo().meleeAttackAttr;
    int damage = getAttr(damageAttr) + weapon->getModifier(damageAttr);
    auto timeSpent = 1_visible;
    AttackLevel attackLevel = Random.choose(getBody().getAttackLevels());
    if (attackParams && attackParams->level)
      attackLevel = *attackParams->level;
    Attack attack(self, attackLevel, weapon->getWeaponInfo().attackType, damage, damageAttr,
        weapon->getWeaponInfo().attackEffect);
    string enemyName = other->getController()->getMessageGenerator().getEnemyName(other);
    if (!canSee(other))
      enemyName = "something";
    weapon->getAttackMsg(this, enemyName);
    other->takeDamage(attack);
    auto movementInfo = *self->spendTime(timeSpent);
    self->addMovementInfo(movementInfo.setDirection(dir).setType(MovementInfo::ATTACK));
  });
}

void Creature::onAttackedBy(WCreature attacker) {
  if (!canSee(attacker))
    unknownAttackers.insert(attacker);
  if (attacker->tribe != tribe)
    privateEnemies.insert(attacker);
  lastAttacker = attacker;
}

constexpr double getDamage(double damageRatio) {
  constexpr double minRatio = 0.6;  // the ratio at which the damage drops to 0
  constexpr double maxRatio = 2.2;     // the ratio at which the damage reaches 1
  constexpr double damageAtOne = 0.12;// damage dealt at a ratio of 1
  if (damageRatio <= minRatio)
    return 0;
  else if (damageRatio <= 1)
    return damageAtOne * (damageRatio - minRatio) / (1.0 - minRatio);
  else if (damageRatio <= maxRatio)
    return damageAtOne + (1.0 - damageAtOne) * (damageRatio - 1.0) / (maxRatio - 1.0);
  else
    return 1.0;
}

bool Creature::takeDamage(const Attack& attack) {
  if (WCreature attacker = attack.attacker) {
    onAttackedBy(attacker);
    if (!attacker->getAttributes().getSkills().hasDiscrete(SkillId::STEALTH))
      for (Position p : visibleCreatures)
        if (p.dist8(position) < 10 && p.getCreature() && !p.getCreature()->isDead())
          p.getCreature()->removeEffect(LastingEffect::SLEEP);
    lastDamageType = getExperienceType(attack.damageType);
  }
  double defense = getAttr(AttrType::DEFENSE);
  for (LastingEffect effect : ENUM_ALL(LastingEffect))
    if (isAffected(effect))
      defense = LastingEffects::modifyCreatureDefense(effect, defense, attack.damageType);
  double damage = getDamage((double) attack.strength / defense);
  if (auto sound = attributes->getAttackSound(attack.type, damage > 0))
    addSound(*sound);
  if (damage > 0) {
    if (attributes->getBody().takeDamage(attack, this, damage))
      return true;
  } else
    you(MsgType::GET_HIT_NODAMAGE);
  if (attack.effect)
    attack.effect->applyToCreature(this, attack.attacker);
  for (LastingEffect effect : ENUM_ALL(LastingEffect))
    if (isAffected(effect))
      LastingEffects::afterCreatureDamage(this, effect);
  return false;
}

static vector<string> extractNames(const vector<AdjectiveInfo>& adjectives) {
  return adjectives.transform([] (const AdjectiveInfo& e) -> string { return e.name; });
}

void Creature::updateViewObject() {
  modViewObject().setCreatureAttributes(ViewObject::CreatureAttributes([this](AttrType t) { return getAttr(t);}));
  modViewObject().setAttribute(ViewObject::Attribute::MORALE, getMorale());
  modViewObject().setModifier(ViewObject::Modifier::DRAW_MORALE);
  modViewObject().getCreatureStatus() = getStatus();
  modViewObject().setGoodAdjectives(combine(extractNames(getGoodAdjectives()), true));
  modViewObject().setBadAdjectives(combine(extractNames(getBadAdjectives()), true));
  getBody().updateViewObject(modViewObject());
  modViewObject().setDescription(getName().title());
  getPosition().setNeedsRenderUpdate(true);
}

double Creature::getMorale() const {
  return min(1.0, max(-1.0, morale + LastingEffects::getMoraleIncrease(this)));
}

void Creature::addMorale(double val) {
  morale = min(1.0, max(-1.0, morale + val));
}

string attrStr(bool strong, bool agile, bool fast) {
  vector<string> good;
  vector<string> bad;
  if (strong)
    good.push_back("strong");
  else
    bad.push_back("weak");
  if (agile)
    good.push_back("agile");
  else
    bad.push_back("clumsy");
  if (fast)
    good.push_back("fast");
  else
    bad.push_back("slow");
  string p1 = combine(good);
  string p2 = combine(bad);
  if (p1.size() > 0 && p2.size() > 0)
    p1.append(", but ");
  p1.append(p2);
  return p1;
}

void Creature::heal(double amount) {
  if (getBody().heal(this, amount))
    clearLastAttacker();
  updateViewObject();
}

void Creature::affectByFire(double amount) {
  if (!isAffected(LastingEffect::FIRE_RESISTANT) &&
      getBody().affectByFire(this, amount)) {
    thirdPerson(getName().the() + " burns to death");
    secondPerson("You burn to death");
    dieWithReason("burnt to death");
  }
}

void Creature::affectBySilver() {
  if (getBody().affectBySilver(this)) {
    you(MsgType::DIE_OF, "silver damage");
    dieWithAttacker(lastAttacker);
  }
}

void Creature::affectByAcid() {
  if (getBody().affectByAcid(this)) {
    you(MsgType::ARE, "dissolved by acid");
    dieWithReason("dissolved by acid");
  }
}

void Creature::poisonWithGas(double amount) {
  if (getBody().affectByPoisonGas(this, amount)) {
    you(MsgType::DIE_OF, "gas poisoning");
    dieWithReason("poisoned with gas");
  }
}

void Creature::setHeld(WCreature c) {
  holding = c->getUniqueId();
}

WCreature Creature::getHoldingCreature() const {
  if (holding)
    for (auto pos : getPosition().neighbors8())
      if (auto c = pos.getCreature())
        if (c->getUniqueId() == *holding)
          return c;
  return nullptr;
}

void Creature::take(vector<PItem> items) {
  for (PItem& elem : items)
    take(std::move(elem));
}

void Creature::take(PItem item) {
  WItem ref = item.get();
  equipment->addItem(std::move(item));
  if (auto action = equip(ref))
    action.perform(this);
}

void Creature::dieWithReason(const string& reason, DropType drops) {
  deathReason = reason;
  dieNoReason(drops);
}

void Creature::dieWithLastAttacker(DropType drops) {
  dieWithAttacker(lastAttacker, drops);
}

vector<PItem> Creature::generateCorpse(bool instantlyRotten) const {
  return getBody().getCorpseItems(getName().bare(), getUniqueId(), instantlyRotten);
}

void Creature::dieWithAttacker(WCreature attacker, DropType drops) {
  CHECK(!isDead()) << getName().bare() << " is already dead. " << getDeathReason().value_or("");
  deathTime = *getGlobalTime();
  lastAttacker = attacker;
  INFO << getName().the() << " dies. Killed by " << (attacker ? attacker->getName().bare() : "");
  getController()->onKilled(attacker);
  if (drops == DropType::EVERYTHING || drops == DropType::ONLY_INVENTORY)
    for (PItem& item : equipment->removeAllItems(this))
      getPosition().dropItem(std::move(item));
  if (drops == DropType::EVERYTHING) {
    getPosition().dropItems(generateCorpse());
    if (auto sound = getBody().getDeathSound())
      addSound(*sound);
  }
  if (attributes->isInnocent())
    getGame()->getStatistics().add(StatId::INNOCENT_KILLED);
  getGame()->getStatistics().add(StatId::DEATH);
  if (attacker)
    attacker->onKilled(this, lastDamageType);
  getGame()->addEvent(EventInfo::CreatureKilled{this, attacker});
  getTribe()->onMemberKilled(this, attacker);
  getLevel()->killCreature(this);
  setController(makeOwner<DoNothingController>(this));
}

void Creature::dieNoReason(DropType drops) {
  dieWithAttacker(nullptr, drops);
}

CreatureAction Creature::flyAway() const {
  if (!isAffected(LastingEffect::FLYING) || getPosition().isCovered())
    return CreatureAction();
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " fly away";
    thirdPerson(getName().the() + " flies away.");
    self->dieNoReason(Creature::DropType::NOTHING);
  });
}

CreatureAction Creature::disappear() const {
  return CreatureAction(this, [=](WCreature self) {
    INFO << getName().the() << " disappears";
    thirdPerson(getName().the() + " disappears.");
    self->dieNoReason(Creature::DropType::NOTHING);
  });
}

CreatureAction Creature::torture(WCreature other) const {
  if (!other->hasCondition(CreatureCondition::RESTRICTED_MOVEMENT) || other->getPosition().dist8(getPosition()) != 1)
    return CreatureAction();
  return CreatureAction(this, [=](WCreature self) {
    thirdPerson(getName().the() + " tortures " + other->getName().the());
    secondPerson("You torture " + other->getName().the());
    if (Random.roll(4)) {
      other->thirdPerson(other->getName().the() + " screams!");
      other->getPosition().unseenMessage("You hear a horrible scream");
    }
    other->getBody().affectByTorture(other);
    getGame()->addEvent(EventInfo::CreatureTortured{other, self});
    self->spendTime();
  });
}

void Creature::surrender(WCreature to) {
  getGame()->addEvent(EventInfo::CreatureSurrendered{this, to});
}

void Creature::retire() {
  if (auto id = attributes->getRetiredViewId())
    modViewObject().setId(*id);
}

void Creature::increaseExpLevel(ExperienceType type, double increase) {
  int curLevel = (int)getAttributes().getExpLevel(type);
  getAttributes().increaseExpLevel(type, increase);
  int newLevel = (int)getAttributes().getExpLevel(type);
  if (curLevel != newLevel) {
    you(MsgType::ARE, "more experienced");
    addPersonalEvent(getName().a() + " reaches " + ::getNameLowerCase(type) + " training level " + toString(newLevel));
  }
  if (type == ExperienceType::SPELL)
    getAttributes().getSpellMap().onExpLevelReached(this, getAttributes().getExpLevel(type));
}

BestAttack Creature::getBestAttack() const {
  return BestAttack(this);
}

CreatureAction Creature::give(WCreature whom, vector<WItem> items) const {
  if (!getBody().isHumanoid() || !whom->canTakeItems(items))
    return CreatureAction(whom->getName().the() + (items.size() == 1 ? " can't take this item."
        : " can't take these items."));
  return CreatureAction(this, [=](WCreature self) {
    for (auto stack : stackItems(items)) {
      thirdPerson(getName().the() + " gives " + whom->getController()->getMessageGenerator().getEnemyName(whom) + " "
          + getPluralAName(stack[0], (int) stack.size()));
      secondPerson("You give " + getPluralTheName(stack[0], (int) stack.size()) + " to " +
          whom->getName().the());
    }
    whom->takeItems(self->equipment->removeItems(items, self), self);
    self->spendTime();
  });
}

CreatureAction Creature::payFor(const vector<WItem>& items) const {
  int totalPrice = std::accumulate(items.begin(), items.end(), 0,
      [](int sum, WConstItem it) { return sum + it->getPrice(); });
  return give(items[0]->getShopkeeper(this), getGold(totalPrice))
      .append([=](WCreature) { for (auto it : items) it->setShopkeeper(nullptr); });
}

CreatureAction Creature::fire(Vec2 direction) const {
  CHECK(direction.length8() == 1);
  if (getEquipment().getItems(ItemIndex::RANGED_WEAPON).empty())
    return CreatureAction("You need a ranged weapon.");
  if (getEquipment().getSlotItems(EquipmentSlot::RANGED_WEAPON).empty())
    return CreatureAction("You need to equip your ranged weapon.");
  if (getBody().numGood(BodyPart::ARM) < 2)
    return CreatureAction("You need two hands to shoot a bow.");
  return CreatureAction(this, [=](WCreature self) {
    auto& weapon = *self->getEquipment().getSlotItems(EquipmentSlot::RANGED_WEAPON).getOnlyElement()
        ->getRangedWeapon();
    weapon.fire(self, direction);
    self->spendTime();
  });
}

void Creature::addMovementInfo(MovementInfo info) {
  modViewObject().addMovementInfo(info);
  getPosition().setNeedsRenderUpdate(true);
}

CreatureAction Creature::whip(const Position& pos) const {
  WCreature whipped = pos.getCreature();
  if (pos.dist8(position) > 1 || !whipped)
    return CreatureAction();
  return CreatureAction(this, [=](WCreature self) {
    thirdPerson(PlayerMessage(getName().the() + " whips " + whipped->getName().the()));
    auto moveInfo = *self->spendTime();
    if (Random.roll(3)) {
      addSound(SoundId::WHIP);
      self->addMovementInfo(moveInfo.setDirection(position.getDir(pos)).setType(MovementInfo::ATTACK));
    }
    if (Random.roll(5)) {
      whipped->thirdPerson(whipped->getName().the() + " screams!");
      whipped->getPosition().unseenMessage("You hear a horrible scream!");
    }
    if (Random.roll(10)) {
      whipped->addMorale(0.05);
      whipped->you(MsgType::FEEL, "happier");
    }
  });
}

void Creature::addSound(const Sound& sound1) const {
  Sound sound(sound1);
  sound.setPosition(getPosition());
  getGame()->getView()->addSound(sound);
}

CreatureAction Creature::construct(Vec2 direction, FurnitureType type) const {
  if (getPosition().plus(direction).canConstruct(type) && canConstruct(type))
    return CreatureAction(this, [=](WCreature self) {
        addSound(Sound(SoundId::DIGGING).setPitch(0.5));
        getPosition().plus(direction).construct(type, self);
        self->spendTime();
      });
  return CreatureAction();
}

bool Creature::canConstruct(FurnitureType) const {
  return attributes->getSkills().hasDiscrete(SkillId::CONSTRUCTION);
}

CreatureAction Creature::eat(WItem item) const {
  return CreatureAction(this, [=](WCreature self) {
    thirdPerson(getName().the() + " eats " + item->getAName());
    secondPerson("You eat " + item->getAName());
    self->addEffect(LastingEffect::SATIATED, 500_visible);
    self->getPosition().removeItem(item);
    self->spendTime(3_visible);
  });
}

void Creature::destroyImpl(Vec2 direction, const DestroyAction& action) {
  auto pos = getPosition().plus(direction);
  if (auto furniture = pos.modFurniture(FurnitureLayer::MIDDLE)) {
    string name = furniture->getName();
    secondPerson("You "_s + action.getVerbSecondPerson() + " the " + name);
    thirdPerson(getName().the() + " " + action.getVerbThirdPerson() + " the " + name);
    pos.unseenMessage(action.getSoundText());
    furniture->tryToDestroyBy(pos, this, action);
  }
}

CreatureAction Creature::destroy(Vec2 direction, const DestroyAction& action) const {
  auto pos = getPosition().plus(direction);
  if (auto furniture = pos.getFurniture(FurnitureLayer::MIDDLE))
    if (direction.length8() <= 1 && furniture->canDestroy(getMovementType(), action))
      return CreatureAction(this, [=](WCreature self) {
        self->destroyImpl(direction, action);
        auto movementInfo = *self->spendTime();
        if (direction.length8() == 1)
          self->addMovementInfo(movementInfo
              .setDirection(getPosition().getDir(pos))
              .setMaxLength(1_visible)
              .setType(MovementInfo::ATTACK));
      });
  return CreatureAction();
}

bool Creature::canCopulateWith(WConstCreature c) const {
  return attributes->getSkills().hasDiscrete(SkillId::COPULATION) &&
      c->getBody().canCopulateWith() &&
      c->attributes->getGender() != attributes->getGender() &&
      c->isAffected(LastingEffect::SLEEP);
}

bool Creature::canConsume(WConstCreature c) const {
  return c->getBody().canConsume() && attributes->getSkills().hasDiscrete(SkillId::CONSUMPTION) && isFriend(c);
}

CreatureAction Creature::copulate(Vec2 direction) const {
  WConstCreature other = getPosition().plus(direction).getCreature();
  if (!other || !canCopulateWith(other))
    return CreatureAction();
  return CreatureAction(this, [=](WCreature self) {
      INFO << getName().bare() << " copulate with " << other->getName().bare();
      you(MsgType::COPULATE, "with " + other->getName().the());
      self->spendTime(2_visible);
    });
}

void Creature::addPersonalEvent(const string& s) {
  if (WModel m = position.getModel())
    m->addEvent(EventInfo::CreatureEvent{this, s});
}

CreatureAction Creature::consume(WCreature other) const {
  if (!other || !canConsume(other) || other->getPosition().dist8(getPosition()) > 1)
    return CreatureAction();
  return CreatureAction(this, [=] (WCreature self) {
    self->attributes->consume(self, *other->attributes);
    other->dieWithAttacker(self, Creature::DropType::ONLY_INVENTORY);
    self->spendTime(2_visible);
  });
}

WItem Creature::getWeapon() const {
  vector<WItem> it = equipment->getSlotItems(EquipmentSlot::WEAPON);
  WItem weapon;
  if (!it.empty())
    weapon = it[0];
  return getBody().chooseWeapon(weapon);
}

CreatureAction Creature::applyItem(WItem item) const {
  if (!contains({ItemClass::TOOL, ItemClass::POTION, ItemClass::FOOD, ItemClass::BOOK, ItemClass::SCROLL},
      item->getClass()) || !getBody().isHumanoid())
    return CreatureAction("You can't apply this item");
  if (getBody().numGood(BodyPart::ARM) == 0)
    return CreatureAction("You have no healthy arms!");
  return CreatureAction(this, [=] (WCreature self) {
      auto time = item->getApplyTime();
      secondPerson("You " + item->getApplyMsgFirstPerson(self));
      thirdPerson(getName().the() + " " + item->getApplyMsgThirdPerson(self));
      position.unseenMessage(item->getNoSeeApplyMsg());
      item->apply(self);
      if (item->isDiscarded()) {
        self->equipment->removeItem(item, self);
      }
      self->spendTime(time);
  });
}

CreatureAction Creature::throwItem(WItem item, Vec2 direction) const {
  if (!getBody().numGood(BodyPart::ARM) || !getBody().isHumanoid())
    return CreatureAction("You can't throw anything!");
  else if (item->getWeight() > 20)
    return CreatureAction(item->getTheName() + " is too heavy!");
  int dist = 0;
  int str = 20;
  if (item->getWeight() <= 0.5)
    dist = 10 * str / 15;
  else if (item->getWeight() <= 5)
    dist = 5 * str / 15;
  else if (item->getWeight() <= 20)
    dist = 2 * str / 15;
  else
    FATAL << "Item too heavy.";
  int damage = getAttr(AttrType::RANGED_DAMAGE) + item->getModifier(AttrType::RANGED_DAMAGE);
  return CreatureAction(this, [=](WCreature self) {
    Attack attack(self, Random.choose(getBody().getAttackLevels()), item->getWeaponInfo().attackType, damage, AttrType::DAMAGE);
    secondPerson("You throw " + item->getAName(false, this));
    thirdPerson(getName().the() + " throws " + item->getAName());
    self->getPosition().throwItem(self->equipment->removeItem(item, self), attack, dist, direction, getVision().getId());
    self->spendTime();
  });
}

bool Creature::canSeeOutsidePosition(WConstCreature c) const {
  return LastingEffects::canSee(this, c);
}

bool Creature::canSeeInPosition(WConstCreature c) const {
  if (!c->getPosition().isSameLevel(position))
    return false;
  return !isAffected(LastingEffect::BLIND) && (!c->isAffected(LastingEffect::INVISIBLE) || isFriend(c)) &&
      (!c->isHidden() || c->knowsHiding(this));
}

bool Creature::canSee(WConstCreature c) const {
  return canSeeInPosition(c) && c->getPosition().isVisibleBy(this);
}

bool Creature::canSee(Position pos) const {
  return !isAffected(LastingEffect::BLIND) && pos.isVisibleBy(this);
}

bool Creature::canSee(Vec2 pos) const {
  return !isAffected(LastingEffect::BLIND) && position.withCoord(pos).isVisibleBy(this);
}

bool Creature::isPlayer() const {
  return getController()->isPlayer();
}

const CreatureName& Creature::getName() const {
  return attributes->getName();
}

CreatureName& Creature::getName() {
  return attributes->getName();
}

const char* Creature::identify() const {
  return getName().identify();
}

TribeSet Creature::getFriendlyTribes() const {
  if (WGame game = getGame())
    return game->getTribe(tribe)->getFriendlyTribes();
  else
    return TribeSet().insert(tribe);
}

MovementType Creature::getMovementType() const {
  return MovementType(getFriendlyTribes(), {
      true,
      isAffected(LastingEffect::FLYING),
      attributes->getSkills().hasDiscrete(SkillId::SWIMMING),
      getBody().canWade()})
    .setDestroyActions(EnumSet<DestroyAction::Type>([this](auto t) { return DestroyAction(t).canNavigate(this); }))
    .setForced(isAffected(LastingEffect::BLIND) || getHoldingCreature() || forceMovement)
    .setFireResistant(isAffected(LastingEffect::FIRE_RESISTANT))
    .setSunlightVulnerable(isAffected(LastingEffect::SUNLIGHT_VULNERABLE) && !isAffected(LastingEffect::DARKNESS_SOURCE)
        && (!getGame() || getGame()->getSunlightInfo().getState() == SunlightState::DAY));
}

int Creature::getDifficultyPoints() const {
  difficultyPoints = max(difficultyPoints,
      getAttr(AttrType::SPELL_DAMAGE) +
      getAttr(AttrType::DEFENSE) + getAttr(AttrType::DAMAGE));
  return difficultyPoints;
}

CreatureAction Creature::continueMoving() {
  if (shortestPath && shortestPath->isReachable(getPosition()))
    return move(shortestPath->getNextMove(getPosition()));
  else
    return CreatureAction();
}

CreatureAction Creature::stayIn(WLevel level, Rectangle area) {
  if (level != getLevel() || !getPosition().getCoord().inRectangle(area)) {
    if (level == getLevel())
      for (Position v : getPosition().neighbors8(Random))
        if (v.getCoord().inRectangle(area))
          if (auto action = move(v))
            return action;
    return moveTowards(Position(area.middle(), getLevel()));
  }
  return CreatureAction();
}

CreatureAction Creature::moveTowards(Position pos, NavigationFlags flags) {
  if (!pos.isValid())
    return CreatureAction();
  if (pos.isSameLevel(position))
    return moveTowards(pos, false, flags);
  else if (auto stairs = position.getStairsTo(pos)) {
    if (stairs == position)
      return applySquare(position);
    else
      return moveTowards(*stairs, false, flags.requireStepOnTile());
  } else
    return CreatureAction();
}

bool Creature::canNavigateTo(Position pos) const {
  MovementType movement = getMovementType();
  for (Position v : pos.neighbors8())
    if (v.isConnectedTo(position, movement))
      return true;
  return false;
}

CreatureAction Creature::moveTowards(Position pos, bool away, NavigationFlags flags) {
  CHECK(pos.isSameLevel(position));
  if (flags.stepOnTile && !pos.canEnterEmpty(this))
    return CreatureAction();
  MEASURE(
  if (!away && !canNavigateTo(pos))
    return CreatureAction();
  , "Creature Sector checking " + getName().bare() + " from " + toString(position) + " to " + toString(pos));
  bool newPath = false;
  bool targetChanged = shortestPath && shortestPath->getTarget().dist8(pos) > getPosition().dist8(pos) / 10;
  if (!shortestPath || targetChanged || shortestPath->isReversed() != away) {
    newPath = true;
    if (!away)
      shortestPath.reset(new LevelShortestPath(this, pos, position));
    else
      shortestPath.reset(new LevelShortestPath(this, pos, position, -1.5));
  }
  CHECK(shortestPath);
  if (shortestPath->isReachable(position))
    if (auto action = move(shortestPath->getNextMove(position)))
      return action;
  /*if (newPath)
    return CreatureAction();*/
  INFO << "Reconstructing shortest path.";
  if (!away)
    shortestPath.reset(new LevelShortestPath(this, pos, position));
  else
    shortestPath.reset(new LevelShortestPath(this, pos, position, -1.5));
  if (shortestPath->isReachable(position)) {
    Position pos2 = shortestPath->getNextMove(position);
    if (auto action = move(pos2))
      return action;
    else {
      if (!pos2.canEnterEmpty(this) && flags.destroy)
        if (auto destroyAction = pos2.getBestDestroyAction(getMovementType()))
            if (auto action = destroy(getPosition().getDir(pos2), *destroyAction))
            return action;
      return CreatureAction();
    }
  } else {
    //INFO << "Cannot move toward " << pos.getCoord();
    return CreatureAction();
  }
}

CreatureAction Creature::moveAway(Position pos, bool pathfinding) {
  CHECK(pos.isSameLevel(position));
  if (pos.dist8(getPosition()) <= 5 && pathfinding)
    if (auto action = moveTowards(pos, true, NavigationFlags().noDestroying()))
      return action;
  pair<Vec2, Vec2> dirs = pos.getDir(getPosition()).approxL1();
  vector<CreatureAction> moves;
  if (auto action = move(dirs.first))
    moves.push_back(action);
  if (auto action = move(dirs.second))
    moves.push_back(action);
  if (moves.size() > 0)
    return moves[Random.get(moves.size())];
  return CreatureAction();
}

bool Creature::atTarget() const {
  return shortestPath && getPosition() == shortestPath->getTarget();
}

bool Creature::isUnknownAttacker(WConstCreature c) const {
  return unknownAttackers.contains(c);
}

const Vision& Creature::getVision() const {
  return *vision;
}

const CreatureDebt& Creature::getDebt() const {
  return *debt;
}

CreatureDebt& Creature::getDebt() {
  return *debt;
}

void Creature::updateVisibleCreatures() {
  int range = FieldOfView::sightRange;
  visibleEnemies.clear();
  visibleCreatures.clear();
  for (WCreature c : position.getAllCreatures(range))
    if (canSee(c) || isUnknownAttacker(c)) {
      visibleCreatures.push_back(c->getPosition());
      if (isEnemy(c))
        visibleEnemies.push_back(c->getPosition());
    }
}

vector<WCreature> Creature::getVisibleEnemies() const {
  vector<WCreature> ret;
  for (Position p : visibleEnemies)
    if (WCreature c = p.getCreature())
      if (!c->isDead())
        ret.push_back(c);
  return ret;
}

vector<WCreature> Creature::getVisibleCreatures() const {
  vector<WCreature> ret;
  for (Position p : visibleCreatures)
    if (WCreature c = p.getCreature())
      if (!c->isDead())
        ret.push_back(c);
  return ret;
}

vector<Position> Creature::getVisibleTiles() const {
  if (isAffected(LastingEffect::BLIND))
    return {};
  else
    return getPosition().getVisibleTiles(getVision());
}

const char* getMoraleText(double morale) {
  if (morale >= 0.7)
    return "Ecstatic";
  if (morale >= 0.2)
    return "Merry";
  if (morale < -0.7)
    return "Depressed";
  if (morale < -0.2)
    return "Unhappy";
  return nullptr;
}

vector<AdjectiveInfo> Creature::getGoodAdjectives() const {
  vector<AdjectiveInfo> ret;
  if (!!attributes->getMoraleSpeedIncrease())
    ret.push_back({"Morale affects speed", ""});
  for (LastingEffect effect : ENUM_ALL(LastingEffect))
    if (isAffected(effect))
      if (const char* name = LastingEffects::getGoodAdjective(effect)) {
        ret.push_back({ name, LastingEffects::getDescription(effect) });
        if (!attributes->isAffectedPermanently(effect))
          ret.back().name += attributes->getRemainingString(effect, *getGlobalTime());
      }
  if (getBody().isUndead())
    ret.push_back({"Undead",
        "Undead creatures don't take regular damage and need to be killed by chopping up or using fire."});
  auto morale = getMorale();
  if (morale > 0)
    if (auto text = getMoraleText(morale))
      ret.push_back({text, "Morale affects minion's productivity and chances of fleeing from battle."});
  return ret;
}

vector<AdjectiveInfo> Creature::getBadAdjectives() const {
  vector<AdjectiveInfo> ret;
  getBody().getBadAdjectives(ret);
  for (LastingEffect effect : ENUM_ALL(LastingEffect))
    if (isAffected(effect))
      if (const char* name = LastingEffects::getBadAdjective(effect)) {
        ret.push_back({ name, LastingEffects::getDescription(effect) });
        if (!attributes->isAffectedPermanently(effect))
          ret.back().name += attributes->getRemainingString(effect, *getGlobalTime());
      }
  auto morale = getMorale();
  if (morale < 0)
    if (auto text = getMoraleText(morale))
      ret.push_back({text, "Morale affects minion's productivity and chances of fleeing from battle."});
  return ret;
}

bool Creature::isSameSector(Position pos) const {
  return pos.isConnectedTo(position, getMovementType());
}

void Creature::setLastCombatIntent(CombatIntentInfo info) {
  lastCombatIntent = info;
}

optional<Creature::CombatIntentInfo> Creature::getLastCombatIntent() const {
  return lastCombatIntent;
}

