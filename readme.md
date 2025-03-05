# Projectile damage system branch
Projectiles can be damaged and pushed by all clients. Server has a more robust way of keeping track of the active projectiles' state.

### To-do:
- Fix Hit_Direct flags for bouncers and projectiles
    - Basic checks for direct shots and damage server side
    - Immediately kill the projectile when direct shots are landed
- Fix only projectile owner being able to push/damage the projectile, other clients can only instantly kill it
- Fix direct hits of the same bouncer type not triggering combo attacks
- Basic server-side cheat prevention
    - Send a network message for projectile explosion from all the clients and check whether the majority confirmed a projectile is dead before forcing the kill on the owner's projectile
- Make sure the origin position of the projectile is always as accurate as possible across all clients to prevent noticeable synchronization issues for the projectiles
- Remove differences between linear projectiles and bouncers collision detection
