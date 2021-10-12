#include "wrappers.h"
#include "utils.h"

player::player(WinProcess& proc, pointer<rust::BasePlayer_o> handle_, const rust::BasePlayer_o& player) {
    this->handle = handle_;
    this->name = player._displayName ? readString8(proc, player._displayName) : std::string{"player"};
    this->health = player._health;
    this->position = getPosition(proc, player.m_CachedPtr);

    auto vec3 = player.input.member(bodyAngles).read(proc);
    this->angles = glm::vec2{vec3.x, vec3.y};
    this->flags = player.playerFlags;

    std::optional item = getHeldItem(proc, handle_);
    if (item) {
        auto def = item->member(info).read(proc);
        /*auto translate = def.member(displayName).read(proc);
        if (translate) {
            auto english = translate.member(english).read(proc);
            auto name = readString8(proc, english);
            this->weaponName = std::move(name);
        }*/
        auto itemName = def.member(shortname).read(proc);
        this->weaponName = readString8(proc, itemName);
    }
}