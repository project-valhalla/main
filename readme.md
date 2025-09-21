# To-do

* \[x] Networked item drops (based on projectiles) for alive players
* \[ ] Networked item drops (based on projectiles) for dead players (server-side)
* \[ ] Disable player collision for dropped items
* \[ ] Disable attacks on dropped items (strangely for now they can be attacked)
* \[x] Add an 'unavailable' bool for collected items, so they can disappear immediately even if not removed by the owner yet
* \[x] Fix dropped items causing client status mismatch
* \[ ] Dropped items should add a custom amount of e.g. ammo based on what their owner had, not based on the map entity's default amount
* \[ ] Send dropped items position to clients when they join mid-game // WONTFIX: not really needed for now, we have respawns anyway
