#include "field.h"

OCG_DuelStatus field::process() {
	core.units.splice(core.units.begin(), core.subunits);
	if(core.units.empty())
		return OCG_DUEL_STATUS_END;
	return std::visit(*this, core.units.front());
}
