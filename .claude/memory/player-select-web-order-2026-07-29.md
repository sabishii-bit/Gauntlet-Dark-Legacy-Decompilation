# Player/select register-web ordering (2026-07-29)

Three register-only residuals fell to source-level web creation order:

- `set_player_default_atts`: remove the redundant `Player* p = vp` local and
  use the function parameter directly. This moves the parameter copy to the
  first nonvolatile web (`r27`). Declare/initialize the loop counter before
  the two cached fields; MWCC then colors `j/index/chartype` as
  `r30/r29/r28`, matching retail.
- `all_players_go_to_same_level`: declaring `i` before the initialized `dest`
  swaps their otherwise identical nonvolatile webs without changing statement
  scheduling.
- `check_active_players`: declaring `p` before initialized `count` swaps the
  two call-crossing webs and makes the function exact.

Practical rule: for opcode-identical loops with a two- or three-local register
cycle, preserve the target's statement order but permute declaration order.
If a copied pointer parameter is colored last, delete the redundant typed
local and use/cast the parameter directly before permuting the remaining
locals.
