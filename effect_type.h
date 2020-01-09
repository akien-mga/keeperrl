#pragma once

#include "util.h"
#include "effect.h"
#include "creature_id.h"
#include "furniture_type.h"
#include "destroy_action.h"
#include "intrinsic_attack.h"
#include "spell_id.h"
#include "sound.h"

RICH_ENUM(FilterType, ALLY, ENEMY);

#define EFFECT_TYPE_INTERFACE \
  void applyToCreature(Creature*, Creature* attacker = nullptr) const;\
  string getName(const ContentFactory*) const;\
  string getDescription(const ContentFactory*) const


#define SIMPLE_EFFECT(Name) \
  struct Name : public EmptyStruct<Name> { \
    EFFECT_TYPE_INTERFACE;\
  }

namespace Effects {

SIMPLE_EFFECT(Escape);
SIMPLE_EFFECT(Teleport);
struct Heal {
  EFFECT_TYPE_INTERFACE;
  HealthType SERIAL(healthType);
  SERIALIZE_ALL(healthType)
};
SIMPLE_EFFECT(Fire);
SIMPLE_EFFECT(Ice);
SIMPLE_EFFECT(DestroyEquipment);
struct DestroyWalls {
  EFFECT_TYPE_INTERFACE;
  DestroyAction::Type SERIAL(action);
  SERIALIZE_ALL(action)
};
struct Enhance {
  EFFECT_TYPE_INTERFACE;
  ItemUpgradeType SERIAL(type);
  int SERIAL(amount);
  const char* typeAsString() const;
  const char* amountAs(const char* positive, const char* negative) const;
  SERIALIZE_ALL(type, amount)
};
struct EmitPoisonGas {
  EFFECT_TYPE_INTERFACE;
  double SERIAL(amount) = 0.8;
  SERIALIZE_ALL(amount)
};
SIMPLE_EFFECT(CircularBlast);
SIMPLE_EFFECT(Deception);
struct Summon {
  EFFECT_TYPE_INTERFACE;
  Summon(CreatureId id, Range c) : creature(id), count(c) {}
  Summon() {}
  CreatureId SERIAL(creature);
  Range SERIAL(count);
  optional<int> SERIAL(ttl);
  SERIALIZE_ALL(creature, count, ttl)
};
struct AssembledMinion {
  EFFECT_TYPE_INTERFACE;
  CreatureId SERIAL(creature);
  SERIALIZE_ALL(creature)
};
struct SummonEnemy {
  EFFECT_TYPE_INTERFACE;
  SummonEnemy(CreatureId id, Range c) : creature(id), count(c) {}
  SummonEnemy() {}
  CreatureId SERIAL(creature);
  Range SERIAL(count);
  optional<int> SERIAL(ttl);
  SERIALIZE_ALL(creature, count, ttl)
};
SIMPLE_EFFECT(SummonElement);
SIMPLE_EFFECT(Acid);
struct Alarm {
  EFFECT_TYPE_INTERFACE;
  bool SERIAL(silent) = false;
  SERIALIZE_ALL(silent)
};
SIMPLE_EFFECT(TeleEnemies);
SIMPLE_EFFECT(SilverDamage);

struct Lasting {
  EFFECT_TYPE_INTERFACE;
  LastingEffect SERIAL(lastingEffect);
  SERIALIZE_ALL(lastingEffect)
};

struct RemoveLasting {
  EFFECT_TYPE_INTERFACE;
  LastingEffect SERIAL(lastingEffect);
  SERIALIZE_ALL(lastingEffect)
};
struct Permanent {
  EFFECT_TYPE_INTERFACE;
  LastingEffect SERIAL(lastingEffect);
  SERIALIZE_ALL(lastingEffect)
};
struct RemovePermanent {
  EFFECT_TYPE_INTERFACE;
  LastingEffect SERIAL(lastingEffect);
  SERIALIZE_ALL(lastingEffect)
};
struct PlaceFurniture {
  EFFECT_TYPE_INTERFACE;
  FurnitureType SERIAL(furniture);
  SERIALIZE_ALL(furniture)
};
struct Damage {
  EFFECT_TYPE_INTERFACE;
  AttrType SERIAL(attr);
  AttackType SERIAL(attackType);
  SERIALIZE_ALL(attr, attackType)
};
struct IncreaseAttr {
  EFFECT_TYPE_INTERFACE;
  AttrType SERIAL(attr);
  int SERIAL(amount);
  const char* get(const char* ifIncrease, const char* ifDecrease) const;
  SERIALIZE_ALL(attr, amount)
};
struct InjureBodyPart {
  EFFECT_TYPE_INTERFACE;
  BodyPart SERIAL(part);
  SERIALIZE_ALL(part)
};
struct LoseBodyPart {
  EFFECT_TYPE_INTERFACE;
  BodyPart SERIAL(part);
  SERIALIZE_ALL(part)
};
struct AddBodyPart {
  EFFECT_TYPE_INTERFACE;
  BodyPart SERIAL(part);
  int SERIAL(count);
  optional<ItemType> SERIAL(attack);
  SERIALIZE_ALL(part, count, attack)
};
SIMPLE_EFFECT(MakeHumanoid);
struct Area {
  EFFECT_TYPE_INTERFACE;
  int SERIAL(radius);
  HeapAllocated<Effect> SERIAL(effect);
  SERIALIZE_ALL(radius, effect)
};
struct CustomArea {
  EFFECT_TYPE_INTERFACE;
  HeapAllocated<Effect> SERIAL(effect);
  vector<Vec2> SERIAL(positions);
  vector<Position> getTargetPos(const Creature* attacker, Position targetPos) const;
  SERIALIZE_ALL(effect, positions)
};
struct RegrowBodyPart {
  EFFECT_TYPE_INTERFACE;
  int SERIAL(maxCount);
  SERIALIZE_ALL(maxCount)
};
struct Suicide {
  EFFECT_TYPE_INTERFACE;
  MsgType SERIAL(message);
  SERIALIZE_ALL(message)
};
SIMPLE_EFFECT(DoubleTrouble);
SIMPLE_EFFECT(Blast);
SIMPLE_EFFECT(Pull);
SIMPLE_EFFECT(Shove);
SIMPLE_EFFECT(SwapPosition);
struct ReviveCorpse {
  EFFECT_TYPE_INTERFACE;
  vector<CreatureId> SERIAL(summoned);
  int SERIAL(ttl);
  SERIALIZE_ALL(summoned, ttl)
};
struct Filter {
  EFFECT_TYPE_INTERFACE;
  bool applies(bool isEnemy) const;
  FilterType SERIAL(filter);
  HeapAllocated<Effect> SERIAL(effect);
  SERIALIZE_ALL(filter, effect)
};
SIMPLE_EFFECT(Wish);
struct Caster {
  EFFECT_TYPE_INTERFACE;
  HeapAllocated<Effect> SERIAL(effect);
  SERIALIZE_ALL(effect)
};
struct Chain {
  EFFECT_TYPE_INTERFACE;
  vector<Effect> SERIAL(effects);
  SERIALIZE_ALL(effects)
};
struct ChooseRandom {
  EFFECT_TYPE_INTERFACE;
  vector<Effect> SERIAL(effects);
  SERIALIZE_ALL(effects)
};
struct Message {
  EFFECT_TYPE_INTERFACE;
  string SERIAL(text);
  SERIALIZE_ALL(text)
};
struct CreatureMessage {
  EFFECT_TYPE_INTERFACE;
  string SERIAL(secondPerson);
  string SERIAL(thirdPerson);
  SERIALIZE_ALL(secondPerson, thirdPerson)
};
struct GrantAbility {
  EFFECT_TYPE_INTERFACE;
  SpellId SERIAL(id);
  SERIALIZE_ALL(id)
};
struct IncreaseMorale {
  EFFECT_TYPE_INTERFACE;
  double SERIAL(amount);
  SERIALIZE_ALL(amount)
};
struct Chance {
  EFFECT_TYPE_INTERFACE;
  double SERIAL(value);
  HeapAllocated<Effect> SERIAL(effect);
  SERIALIZE_ALL(value, effect)
};
SIMPLE_EFFECT(TriggerTrap);
struct AnimateItems {
  EFFECT_TYPE_INTERFACE;
  int SERIAL(maxCount);
  int SERIAL(radius);
  Range SERIAL(time);
  SERIALIZE_ALL(maxCount, radius, time)
};
struct DropItems {
  EFFECT_TYPE_INTERFACE;
  ItemType SERIAL(type);
  Range SERIAL(count);
  SERIALIZE_ALL(type, count)
};
struct SoundEffect {
  EFFECT_TYPE_INTERFACE;
  Sound SERIAL(sound);
  SERIALIZE_ALL(sound)
};

#define EFFECT_TYPES_LIST\
  X(Escape, 0)\
  X(Teleport, 1)\
  X(Heal, 2)\
  X(Fire, 3)\
  X(Ice, 4)\
  X(DestroyEquipment, 5)\
  X(Enhance, 6)\
  X(Suicide, 7)\
  X(IncreaseAttr, 8)\
  X(EmitPoisonGas, 9)\
  X(CircularBlast, 10)\
  X(Deception, 11)\
  X(Summon, 12)\
  X(SummonElement, 13)\
  X(Acid, 14)\
  X(Alarm, 15)\
  X(TeleEnemies, 16)\
  X(SilverDamage, 17)\
  X(DoubleTrouble, 18)\
  X(Lasting, 19)\
  X(RemoveLasting, 20)\
  X(Permanent, 21)\
  X(RemovePermanent, 22)\
  X(PlaceFurniture, 23)\
  X(Damage, 24)\
  X(InjureBodyPart, 25)\
  X(LoseBodyPart, 26)\
  X(RegrowBodyPart, 27)\
  X(AddBodyPart, 28)\
  X(DestroyWalls, 29)\
  X(Area, 30)\
  X(CustomArea, 31)\
  X(ReviveCorpse, 32)\
  X(Blast, 33)\
  X(Pull, 34)\
  X(Shove, 35)\
  X(SwapPosition, 36)\
  X(Filter, 37)\
  X(SummonEnemy, 38)\
  X(Wish, 39)\
  X(Chain, 40)\
  X(ChooseRandom, 41)\
  X(Caster, 42)\
  X(IncreaseMorale, 43)\
  X(Message, 44)\
  X(Chance, 45)\
  X(AssembledMinion, 46)\
  X(TriggerTrap, 47)\
  X(AnimateItems, 48)\
  X(MakeHumanoid, 49)\
  X(GrantAbility, 50)\
  X(CreatureMessage, 51)\
  X(SoundEffect, 52)\
  X(DropItems, 53)


struct EffectType {
  int index;
  template <typename T>
  optional<T> getValueMaybe() const {
    return none;
  }
  template <typename T>
  optional<T&> getReferenceMaybe() const {
    return none;
  }
  template <typename T>
  bool contains() const {
    return !!getReferenceMaybe<T>();
  }
  EffectType() : index(0), elem0(Escape{}) {}
#define X(Type, Index) \
  EffectType(Type&& t) : index(Index), elem##Index(std::move(t)) {}\
  EffectType(const Type& t) : index(Index), elem##Index(t) {}\
  template<>\
  optional<Type> getValueMaybe<Type>() const {\
    if (index == Index)\
      return elem##Index;\
    return none;\
  }\
  template<>\
  optional<const Type&> getReferenceMaybe() const {\
    if (index == Index)\
      return elem##Index;\
    return none;\
  }
  EFFECT_TYPES_LIST
#undef X

  template<typename... Fs>
  auto visit(Fs... fs) const {
    auto f = variant_helpers::LambdaVisitor<Fs...>(fs...);
    switch (index) {
#define X(Type, Index)\
      case Index: return f(elem##Index); break;
      EFFECT_TYPES_LIST
#undef X
      default: fail();
    }
  }
  EffectType(const EffectType& t) : index(t.index) {
    switch (index) {
#define X(Type, Index)\
      case Index: new(&elem##Index) Type(t.elem##Index); break;
      EFFECT_TYPES_LIST
#undef X
      default: fail();
    }
  }
  EffectType(EffectType&& t) : index(t.index) {
    switch (index) {
#define X(Type, Index)\
      case Index: new(&elem##Index) Type(std::move(t.elem##Index)); break;
      EFFECT_TYPES_LIST
#undef X
      default: fail();
    }
  }
  EffectType& operator = (const EffectType& t) {
    if (index == t.index)
      switch (index) {
  #define X(Type, Index)\
        case Index: elem##Index = t.elem##Index; break;
        EFFECT_TYPES_LIST
  #undef X
        default: fail();
      }
    else {
      this->~EffectType();
      new (this) EffectType(t);
    }
    return *this;
  }
  EffectType& operator = (EffectType&& t) {
    if (index == t.index)
      switch (index) {
  #define X(Type, Index)\
        case Index: elem##Index = std::move(t.elem##Index); break;
        EFFECT_TYPES_LIST
  #undef X
        default: fail();
      }
    else {
      this->~EffectType();
      new (this) EffectType(std::move(t));
    }
    return *this;
  }
  ~EffectType() {
    switch (index) {
#define X(Type, Index)\
      case Index: elem##Index.~Type(); break;
      EFFECT_TYPES_LIST
#undef X
      default: fail();
    }
  }
  union {
#define X(Type, Index) \
    Type elem##Index;
    EFFECT_TYPES_LIST
#undef X
  };
};

}

class EffectType : public Effects::EffectType {
  public:
  using Effects::EffectType::EffectType;
};

template <class Archive>
void serialize(Archive& ar1, EffectType& v);
