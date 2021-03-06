/*
 * libdebug.cpp
 *
 *  Created on: 2012-2-8
 *      Author: Argon
 */

#include <string.h>
#include "scriptlib.h"
#include "duel.h"
#include "field.h"
#include "card.h"

int32 scriptlib::debug_message(lua_State* L) {
	const auto pduel = lua_get<duel*>(L);
	lua_getglobal(L, "tostring");
	lua_pushvalue(L, -2);
	lua_pcall(L, 1, 1, 0);
	pduel->handle_message(lua_tostring_or_empty(L, -1), OCG_LOG_TYPE_FROM_SCRIPT);
	return 0;
}
int32 scriptlib::debug_add_card(lua_State* L) {
	check_param_count(L, 6);
	const auto pduel = lua_get<duel*>(L);
	auto code = lua_get<uint32>(L, 1);
	auto owner = lua_get<uint8>(L, 2);
	auto playerid = lua_get<uint8>(L, 3);
	auto location = lua_get<uint16>(L, 4);
	auto sequence = lua_get<uint16>(L, 5);
	auto position = lua_get<uint8>(L, 6);
	bool proc = lua_get<bool, false>(L, 7);
	if(owner != 0 && owner != 1)
		return 0;
	if(playerid != 0 && playerid != 1)
		return 0;
	if(pduel->game_field->is_location_useable(playerid, location, sequence)) {
		card* pcard = pduel->new_card(code);
		pcard->owner = owner;
		if(location == LOCATION_EXTRA && (position == 0 || (pcard->data.type & TYPE_PENDULUM) == 0))
			position = POS_FACEDOWN_DEFENSE;
		pcard->sendto_param.position = position;
		if(location == LOCATION_PZONE) {
			int32 seq = pduel->game_field->get_pzone_index(sequence);
			pduel->game_field->add_card(playerid, pcard, LOCATION_SZONE, seq, TRUE);
		} else if(location == LOCATION_FZONE) {
			int32 loc = LOCATION_SZONE;
			pduel->game_field->add_card(playerid, pcard, loc, 5);
		} else
			pduel->game_field->add_card(playerid, pcard, location, sequence);
		pcard->current.position = position;
		if(!(location & (LOCATION_ONFIELD | LOCATION_PZONE)) || (position & POS_FACEUP)) {
			pcard->enable_field_effect(true);
			pduel->game_field->adjust_instant();
		}
		if(proc)
			pcard->set_status(STATUS_PROC_COMPLETE, TRUE);
		interpreter::pushobject(L, pcard);
		return 1;
	} else if(location & LOCATION_ONFIELD) {
		card* pcard = pduel->new_card(code);
		pcard->owner = owner;
		card* fcard = pduel->game_field->get_field_card(playerid, location, sequence);
		fcard->xyz_add(pcard);
		if(proc)
			pcard->set_status(STATUS_PROC_COMPLETE, TRUE);
		interpreter::pushobject(L, pcard);
		return 1;
	}
	return 0;
}
int32 scriptlib::debug_set_player_info(lua_State* L) {
	check_param_count(L, 4);
	const auto pduel = lua_get<duel*>(L);
	auto playerid = lua_get<uint8>(L, 1);
	auto lp = lua_get<uint32>(L, 2);
	auto startcount = lua_get<uint32>(L, 3);
	auto drawcount = lua_get<uint32>(L, 4);
	if(playerid != 0 && playerid != 1)
		return 0;
	pduel->game_field->player[playerid].lp = lp;
	pduel->game_field->player[playerid].start_lp = lp;
	pduel->game_field->player[playerid].start_count = startcount;
	pduel->game_field->player[playerid].draw_count = drawcount;
	return 0;
}
int32 scriptlib::debug_pre_summon(lua_State* L) {
	check_param_count(L, 2);
	auto pcard = lua_get<card*, true>(L, 1);
	auto summon_type = lua_get<uint32>(L, 2);
	auto summon_location = lua_get<uint8, 0>(L, 3);
	pcard->summon_info = summon_type | (summon_location << 16);
	return 0;
}
int32 scriptlib::debug_pre_equip(lua_State* L) {
	check_param_count(L, 2);
	auto equip_card = lua_get<card*, true>(L, 1);
	auto target = lua_get<card*, true>(L, 2);
	if((equip_card->current.location != LOCATION_SZONE)
	        || (target->current.location != LOCATION_MZONE)
	        || (target->current.position & POS_FACEDOWN))
		lua_pushboolean(L, 0);
	else {
		equip_card->equip(target, FALSE);
		equip_card->effect_target_cards.insert(target);
		target->effect_target_owner.insert(equip_card);
		lua_pushboolean(L, 1);
	}
	return 1;
}
int32 scriptlib::debug_pre_set_target(lua_State* L) {
	check_param_count(L, 2);
	auto t_card = lua_get<card*, true>(L, 1);
	auto target = lua_get<card*, true>(L, 2);
	t_card->add_card_target(target);
	return 0;
}
int32 scriptlib::debug_pre_add_counter(lua_State* L) {
	check_param_count(L, 2);
	auto pcard = lua_get<card*, true>(L, 1);
	auto countertype = lua_get<uint16>(L, 2);
	auto count = lua_get<uint16>(L, 3);
	uint16 cttype = countertype & ~COUNTER_NEED_ENABLE;
	auto pr = pcard->counters.emplace(cttype, card::counter_map::mapped_type());
	auto cmit = pr.first;
	if(pr.second) {
		cmit->second[0] = 0;
		cmit->second[1] = 0;
	}
	if((countertype & COUNTER_WITHOUT_PERMIT) && !(countertype & COUNTER_NEED_ENABLE))
		cmit->second[0] += count;
	else
		cmit->second[1] += count;
	return 0;
}
int32 scriptlib::debug_reload_field_begin(lua_State* L) {
	check_param_count(L, 1);
	const auto pduel = lua_get<duel*>(L);
	auto flag = lua_get<uint64>(L, 1);
	auto rule = lua_get<uint8, 3>(L, 2);
	bool build = lua_get<bool, false>(L, 3);
	pduel->clear();
#define CHECK(MR) case MR : { flag |= DUEL_MODE_MR##MR; break; }
	if(rule && !build) {
		switch (rule) {
		CHECK(1)
		CHECK(2)
		CHECK(3)
		CHECK(4)
		CHECK(5)
		}
#undef CHECK
	}
	pduel->game_field->core.duel_options = flag;
	return 0;
}
int32 scriptlib::debug_reload_field_end(lua_State* L) {
	const auto pduel = lua_get<duel*>(L);
	pduel->game_field->core.shuffle_hand_check[0] = FALSE;
	pduel->game_field->core.shuffle_hand_check[1] = FALSE;
	pduel->game_field->core.shuffle_deck_check[0] = FALSE;
	pduel->game_field->core.shuffle_deck_check[1] = FALSE;
	pduel->game_field->reload_field_info();
	return 0;
}
int32 scriptlib::debug_set_ai_name(lua_State* L) {
	check_param_count(L, 1);
	check_param(L, PARAM_TYPE_STRING, 1);
	const auto pduel = lua_get<duel*>(L);
	auto message = pduel->new_message(MSG_AI_NAME);
	size_t len = 0;
	const char* pstr = lua_tolstring(L, 1, &len);
	if(len > 100)
		len = 100;
	message->write<uint16>(static_cast<uint16>(len));
	message->write(pstr, len);
	message->write<uint8>(0);
	return 0;
}
int32 scriptlib::debug_show_hint(lua_State* L) {
	check_param_count(L, 1);
	check_param(L, PARAM_TYPE_STRING, 1);
	const auto pduel = lua_get<duel*>(L);
	auto message = pduel->new_message(MSG_SHOW_HINT);
	size_t len = 0;
	const char* pstr = lua_tolstring(L, 1, &len);
	if(len > 1024)
		len = 1024;
	message->write<uint16>(static_cast<uint16>(len));
	message->write(pstr, len);
	message->write<uint8>(0);
	return 0;
}

int32 scriptlib::debug_print_stacktrace(lua_State* L) {
	interpreter::print_stacktrace(L);
	return 0;
}
