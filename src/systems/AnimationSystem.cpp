#include "systems/AnimationSystem.h"

#include "components/Animation.h"
#include "components/Sprite.h"

void AnimationSystem::update(World& world, const SpriteSheet& sheet, float dtSeconds)
{
    world.forEach<Animation, Sprite>(
        [&](Entity /*entity*/, Animation& anim, Sprite& sprite)
        {
            if (anim.currentAnim.empty() || anim.finished)
            {
                return;
            }

            const int totalFrames = sheet.frameCount(anim.currentAnim);
            if (totalFrames <= 1)
            {
                sprite.uvRect = sheet.uvRect(anim.currentAnim);
                return;
            }

            anim.frameTimer += dtSeconds;
            if (anim.frameTimer >= anim.frameDuration)
            {
                anim.frameTimer -= anim.frameDuration;
                anim.currentFrame++;

                if (anim.currentFrame >= totalFrames)
                {
                    if (anim.looping)
                    {
                        anim.currentFrame = 0;
                    }
                    else
                    {
                        anim.currentFrame = totalFrames - 1;
                        anim.finished = true;
                    }
                }
            }

            sprite.uvRect = sheet.frameUv(anim.currentAnim, anim.currentFrame);
        });
}
