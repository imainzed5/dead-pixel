// Builds the world for a run: map, entities, loot, survivors and legacy traces.
#include "core/GameShared.h"

using namespace gameshared;

void Game::setupScene()
{
    const GLuint sheetTex = mSpriteSheet.textureId();
    std::mt19937 runRng(mSaveManager.currentRunSeed());

    mPlayerEntity = mWorld.createEntity();

    Transform playerTransform{};
    playerTransform.x = static_cast<float>(mSpawnData.playerStart.x * mTileMap.tileWidth());
    playerTransform.y = static_cast<float>(mSpawnData.playerStart.y * mTileMap.tileHeight());

    Sprite playerSprite{};
    playerSprite.textureId = sheetTex;
    playerSprite.uvRect = mSpriteSheet.uvRect("player_idle");
    playerSprite.color = glm::vec4(1.0f);
    playerSprite.layer = 10;
    playerSprite.drawSize = SpriteSizes::kHumanoid;

    Animation playerAnim{};
    playerAnim.currentAnim = "player_idle";
    playerAnim.frameDuration = 0.15f;

    mWorld.addComponent<Transform>(mPlayerEntity, playerTransform);
    mWorld.addComponent<Sprite>(mPlayerEntity, playerSprite);
    mWorld.addComponent<Animation>(mPlayerEntity, playerAnim);
    mWorld.addComponent<Velocity>(mPlayerEntity, Velocity{});
    mWorld.addComponent<Facing>(mPlayerEntity, Facing{});
    mWorld.addComponent<PlayerInput>(mPlayerEntity, PlayerInput{});
    mWorld.addComponent<Stamina>(mPlayerEntity, Stamina{100.0f, 100.0f, 30.0f, 100.0f});
    mWorld.addComponent<Combat>(mPlayerEntity, Combat{});
    mWorld.addComponent<Health>(mPlayerEntity, Health{100.0f, 100.0f});
    mWorld.addComponent<Needs>(mPlayerEntity, Needs{});

    // Inventory with starting items
    {
        Inventory playerInv{};
        mWorld.addComponent<Inventory>(mPlayerEntity, playerInv);
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

        int startBandages = 2;
        int startFood = 3;
        int startWater = 2;
        switch (saveThreatModeFromIndex(mTitleModeIndex))
        {
        case SaveThreatMode::Stalker:
            startBandages = 3;
            startFood = 4;
            startWater = 3;
            break;
        case SaveThreatMode::OverwhelmingSwarm:
        case SaveThreatMode::Hybrid:
            startBandages = 1;
            startFood = 2;
            startWater = 1;
            break;
        default:
            break;
        }

        InventorySystem::addItem(inv, mItemDatabase, 5, 1);  // knife
        InventorySystem::addItem(inv, mItemDatabase, 4, startBandages);
        InventorySystem::addItem(inv, mItemDatabase, 0, startFood);
        InventorySystem::addItem(inv, mItemDatabase, 3, startWater);
    }
    mWorld.addComponent<MentalState>(mPlayerEntity, MentalState{});
    mWorld.addComponent<Wounds>(mPlayerEntity, Wounds{});

    // Assign traits from seed
    {
        const std::uint32_t traitSeed = mSaveManager.currentRunSeed() ^ 0xDEADBEEFu;
        Traits traits{};
        traits.positive = static_cast<PositiveTrait>(1 + (traitSeed % 3));         // 1-3
        traits.negative = static_cast<NegativeTrait>(1 + ((traitSeed / 3) % 3));   // 1-3
        mWorld.addComponent<Traits>(mPlayerEntity, traits);

        // Apply trait effects
        if (traits.positive == PositiveTrait::Tough)
        {
            Health& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.maximum += 20.0f;
            h.current = h.maximum;
        }
        // Quick and LightSleeper are applied dynamically in movement/sleep systems
    }

    // --- Zombies ---
    std::uniform_real_distribution<float> hpDist(30.0f, 70.0f);
    std::uniform_real_distribution<float> speedMul(0.8f, 1.2f);

    for (const auto& tile : mSpawnData.zombieSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;

        EntityFactory::ZombieParams params;
        params.topLeftWorld = glm::vec2(static_cast<float>(tile.x * mTileMap.tileWidth()),
                                        static_cast<float>(tile.y * mTileMap.tileHeight()));
        params.health = hpDist(runRng);
        params.baseMoveSpeed = 112.0f * speedMul(runRng);
        params.spawnDay = mCurrentDay;
        EntityFactory::spawnZombie(mWorld, mSpriteSheet, params);
    }

    // --- NPC Survivors ---
    {
        static const char* kSurvivorNames[] = {
            "Alex", "Jordan", "Casey", "Morgan", "Riley",
            "Sam", "Quinn", "Avery", "Blake", "Cameron"
        };
        constexpr int kNameCount = 10;

        // Load persisted survivors or spawn fresh ones
        auto savedSurvivors = mSaveManager.loadSurvivors();
        if (!savedSurvivors.empty())
        {
            for (const auto& sr : savedSurvivors)
            {
                if (!sr.alive) continue;
                Entity npc = mWorld.createEntity();
                Transform nt{}; nt.x = sr.x; nt.y = sr.y;
                Sprite ns{}; ns.textureId = sheetTex; ns.uvRect = mSpriteSheet.uvRect("player_idle"); ns.color = glm::vec4(0.7f, 0.85f, 1.0f, 1.0f); ns.layer = 9; ns.drawSize = SpriteSizes::kHumanoid;
                Animation na{}; na.currentAnim = "player_idle"; na.frameDuration = 0.2f;
                SurvivorAI sai{};
                sai.name = sr.name;
                sai.spawnDay = sr.spawnDay;
                sai.hunger = sr.hunger;
                sai.thirst = sr.thirst;
                sai.personality.aggression = sr.aggression;
                sai.personality.cooperation = sr.cooperation;
                sai.personality.competence = sr.competence;
                sai.homeDistrictId = sr.homeDistrictId;
                mWorld.addComponent<Transform>(npc, nt);
                mWorld.addComponent<Sprite>(npc, ns);
                mWorld.addComponent<Animation>(npc, na);
                mWorld.addComponent<Velocity>(npc, Velocity{});
                mWorld.addComponent<SurvivorAI>(npc, sai);
                mWorld.addComponent<Health>(npc, Health{60.0f, 60.0f});
            }
        }
        else
        {
            // Spawn 2-5 fresh survivors in non-wilderness districts
            std::uniform_int_distribution<int> npcCountDist(2, 5);
            const int npcCount = npcCountDist(runRng);

            const auto& layout = mMapGenerator.layoutData();
            std::vector<int> validDistricts;
            for (const auto& dd : layout.districts)
            {
                if (dd.type != static_cast<int>(DistrictType::Wilderness))
                    validDistricts.push_back(dd.id);
            }

            for (int i = 0; i < npcCount; ++i)
            {
                int districtId = 0;
                float spawnX = 200.0f;
                float spawnY = 200.0f;

                if (!validDistricts.empty())
                {
                    districtId = validDistricts[runRng() % validDistricts.size()];
                    for (const auto& dd : layout.districts)
                    {
                        if (dd.id == districtId)
                        {
                            std::uniform_int_distribution<int> xDist(dd.minX + 1, std::max(dd.minX + 1, dd.maxX - 1));
                            std::uniform_int_distribution<int> yDist(dd.minY + 1, std::max(dd.minY + 1, dd.maxY - 1));
                            int tx = xDist(runRng);
                            int ty = yDist(runRng);
                            // Find non-solid tile nearby
                            for (int attempts = 0; attempts < 10; ++attempts)
                            {
                                if (!mTileMap.isSolid(tx, ty)) break;
                                tx = xDist(runRng);
                                ty = yDist(runRng);
                            }
                            spawnX = static_cast<float>(tx * mTileMap.tileWidth());
                            spawnY = static_cast<float>(ty * mTileMap.tileHeight());
                            break;
                        }
                    }
                }

                Entity npc = mWorld.createEntity();
                Transform nt{}; nt.x = spawnX; nt.y = spawnY;
                Sprite ns{}; ns.textureId = sheetTex; ns.uvRect = mSpriteSheet.uvRect("player_idle"); ns.color = glm::vec4(0.7f, 0.85f, 1.0f, 1.0f); ns.layer = 9; ns.drawSize = SpriteSizes::kHumanoid;
                Animation na{}; na.currentAnim = "player_idle"; na.frameDuration = 0.2f;

                SurvivorAI sai{};
                sai.name = kSurvivorNames[(runRng() % kNameCount)];
                sai.spawnDay = mCurrentDay;
                sai.homeDistrictId = districtId;

                std::uniform_real_distribution<float> traitDist(0.0f, 1.0f);
                sai.personality.aggression = traitDist(runRng);
                sai.personality.cooperation = traitDist(runRng);
                sai.personality.competence = traitDist(runRng);

                mWorld.addComponent<Transform>(npc, nt);
                mWorld.addComponent<Sprite>(npc, ns);
                mWorld.addComponent<Animation>(npc, na);
                mWorld.addComponent<Velocity>(npc, Velocity{});
                mWorld.addComponent<SurvivorAI>(npc, sai);
                mWorld.addComponent<Health>(npc, Health{60.0f, 60.0f});
            }
        }
    }

    // Apply bloodline trait selected from persistent legacy history.
    if (mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        traits.bloodline = mSaveManager.selectBloodlineForNewRun();

        if (traits.bloodline == BloodlineTrait::Scavenger && mWorld.hasComponent<Inventory>(mPlayerEntity))
        {
            Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
            InventorySystem::addItem(inv, mItemDatabase, 0, 1);
            InventorySystem::addItem(inv, mItemDatabase, 3, 1);
        }

        if (traits.bloodline == BloodlineTrait::Vigilant && mWorld.hasComponent<MentalState>(mPlayerEntity))
        {
            MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
            mental.panicDecayPerSecond += 0.5f;
        }
    }

    // Initialize district infection system
    mDistrictInfectionSystem.initialize(mMapGenerator.layoutData());
    mThreatDirector.reset(mSaveManager.currentRunSeed(), threatDirectorTuning(saveThreatModeFromIndex(mTitleModeIndex)));
    mFlashlightOn = false;
    mHudDistrictId = -1;
    mDistrictToastTimer = 0.0f;
    mStructureView.snapRoofs(playerTile());

    const auto& layoutDistricts = mMapGenerator.layoutData().districts;
    auto getDistrictType = [&](int tileX, int tileY) -> DistrictType
    {
        const int distId = mTileMap.getDistrictId(tileX, tileY);
        if (distId == 0) return DistrictType::Wilderness;
        for (const auto& def : layoutDistricts)
        {
            if (def.id == distId) return static_cast<DistrictType>(def.type);
        }
        return DistrictType::Wilderness;
    };

    // --- Food ---
    static const std::array<const char*, 3> foodSprites = {"item_canned_food", "item_apple", "item_bread"};
    std::uniform_real_distribution<float> foodRestore(24.0f, 40.0f);

    for (const auto& tile : mSpawnData.foodSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity food = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(foodSprites[runRng() % foodSprites.size()]); s.color = glm::vec4(1.0f); s.layer = 6; s.drawSize = SpriteSizes::kPickup;
        Interactable inter{}; inter.type = InteractableType::FoodPickup; inter.hungerRestore = foodRestore(runRng); inter.prompt = "Scavenge food";
        mWorld.addComponent<Transform>(food, t);
        mWorld.addComponent<Sprite>(food, s);
        mWorld.addComponent<Interactable>(food, inter);
    }

    // --- Water ---
    std::uniform_real_distribution<float> waterRestoreDist(30.0f, 45.0f);
    for (const auto& tile : mSpawnData.waterSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity water = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("item_water_bottle"); s.color = glm::vec4(1.0f); s.layer = 6; s.drawSize = SpriteSizes::kPickup;
        Interactable inter{}; inter.type = InteractableType::WaterPickup; inter.thirstRestore = waterRestoreDist(runRng); inter.prompt = "Collect water";
        mWorld.addComponent<Transform>(water, t);
        mWorld.addComponent<Sprite>(water, s);
        mWorld.addComponent<Interactable>(water, inter);
    }

    // --- Weapons ---
    struct WeaponDef { WeaponCategory category; const char* sprite; };
    static const std::array<WeaponDef, 4> weaponDefs = {{
        {WeaponCategory::Bladed, "item_knife"},
        {WeaponCategory::Blunt, "item_bat"},
        {WeaponCategory::Blunt, "item_pipe"},
        {WeaponCategory::Bladed, "item_axe"},
    }};
    for (const auto& tile : mSpawnData.weaponSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        const auto& wd = weaponDefs[runRng() % weaponDefs.size()];
        Entity wpn = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(wd.sprite); s.color = glm::vec4(1.0f); s.layer = 6; s.drawSize = SpriteSizes::kPickup;
        Interactable inter{}; inter.type = InteractableType::WeaponPickup; inter.weaponCategory = static_cast<int>(wd.category); inter.prompt = "Take weapon";
        mWorld.addComponent<Transform>(wpn, t);
        mWorld.addComponent<Sprite>(wpn, s);
        mWorld.addComponent<Interactable>(wpn, inter);
    }

    // --- Bandages ---
    for (const auto& tile : mSpawnData.bandageSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity bandage = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("item_bandage"); s.color = glm::vec4(1.0f); s.layer = 6; s.drawSize = SpriteSizes::kPickup;
        Interactable inter{}; inter.type = InteractableType::BandagePickup; inter.prompt = "Take bandage";
        mWorld.addComponent<Transform>(bandage, t);
        mWorld.addComponent<Sprite>(bandage, s);
        mWorld.addComponent<Interactable>(bandage, inter);
    }

    // --- Medical caches ---
    {
        std::uniform_int_distribution<int> extraBandageDist(1, 2);
        std::uniform_int_distribution<int> extraAntibioticDist(1, 2);

        for (const auto& tile : mSpawnData.medicalCacheSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            Entity cache = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("env_shelf"); s.color = glm::vec4(0.85f, 0.9f, 0.95f, 1.0f); s.layer = 5;
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Raid med cache";
            Inventory cacheInv{};

            mWorld.addComponent<Transform>(cache, t);
            mWorld.addComponent<Sprite>(cache, s);
            mWorld.addComponent<Interactable>(cache, inter);
            mWorld.addComponent<Inventory>(cache, cacheInv);

            Inventory& inv = mWorld.getComponent<Inventory>(cache);
            InventorySystem::addItem(inv, mItemDatabase, 22, 1);
            InventorySystem::addItem(inv, mItemDatabase, 4, extraBandageDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 15, extraAntibioticDist(runRng));
        }
    }

    // --- Supply caches ---
    {
        std::uniform_int_distribution<int> foodQtyDist(2, 4);
        std::uniform_int_distribution<int> waterQtyDist(1, 3);
        std::uniform_int_distribution<int> materialQtyDist(1, 3);

        for (const auto& tile : mSpawnData.supplyCacheSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            Entity cache = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("env_crate"); s.color = glm::vec4(0.95f, 0.85f, 0.65f, 1.0f); s.layer = 5;
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Raid supply cache";
            Inventory cacheInv{};

            mWorld.addComponent<Transform>(cache, t);
            mWorld.addComponent<Sprite>(cache, s);
            mWorld.addComponent<Interactable>(cache, inter);
            mWorld.addComponent<Inventory>(cache, cacheInv);

            Inventory& inv = mWorld.getComponent<Inventory>(cache);
            InventorySystem::addItem(inv, mItemDatabase, 0, foodQtyDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 3, waterQtyDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 9, materialQtyDist(runRng));
            InventorySystem::addItem(inv, mItemDatabase, 11, 1);
        }
    }

    // --- Sleeping bags ---
    for (const auto& tile : mSpawnData.sleepSpawns)
    {
        if (mTileMap.isSolid(tile.x, tile.y)) continue;
        Entity bed = mWorld.createEntity();
        Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect("item_sleeping_bag"); s.color = glm::vec4(1.0f); s.layer = 5;
        Interactable inter{}; inter.type = InteractableType::SleepSpot; inter.sleepHours = 6.0f; inter.sleepQuality = 0.8f; inter.prompt = "Sleep";
        mWorld.addComponent<Transform>(bed, t);
        mWorld.addComponent<Sprite>(bed, s);
        mWorld.addComponent<Interactable>(bed, inter);
    }

    // --- Home furniture dressing pass (visual-first worldbuilding) ---
    {
        const std::array<const char*, 10> residentialFurniture = {
            "env_bed", "env_sleeping_area", "env_shelf", "env_crate", "env_shelf", "env_bed", "env_chair", "env_table", "env_cabinet", "env_couch"
        };
        const std::array<const char*, 7> commercialFurniture = {
            "env_shelf", "env_crate", "env_shelf", "env_barricade", "env_crate", "env_table", "env_chair"
        };
        const std::array<const char*, 5> industrialFurniture = {
            "env_crate", "env_shelf", "env_barricade", "env_noise_trap", "env_crate"
        };
        const std::array<const char*, 4> wildernessFurniture = {
            "env_sleeping_area", "env_crate", "env_shelf", "env_barricade"
        };

        const auto spawnFurniture = [&](int tileX, int tileY, const char* spriteName, const glm::vec4& color, int layer)
        {
            Entity decor = mWorld.createEntity();
            Transform t{};
            t.x = static_cast<float>(tileX * mTileMap.tileWidth());
            t.y = static_cast<float>(tileY * mTileMap.tileHeight());
            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect(spriteName);
            s.color = color;
            s.layer = layer;
            mWorld.addComponent<Transform>(decor, t);
            mWorld.addComponent<Sprite>(decor, s);
        };

        int furnitureIdx = 0;
        for (const auto& tile : mSpawnData.homeFurnitureSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* spriteName = "env_crate";
            glm::vec4 tint = glm::vec4(0.88f, 0.84f, 0.78f, 1.0f);

            switch (dt)
            {
            case DistrictType::Residential:
                spriteName = residentialFurniture[furnitureIdx % residentialFurniture.size()];
                tint = glm::vec4(0.93f, 0.88f, 0.80f, 1.0f);
                break;
            case DistrictType::Commercial:
                spriteName = commercialFurniture[furnitureIdx % commercialFurniture.size()];
                tint = glm::vec4(0.82f, 0.86f, 0.90f, 1.0f);
                break;
            case DistrictType::Industrial:
                spriteName = industrialFurniture[furnitureIdx % industrialFurniture.size()];
                tint = glm::vec4(0.82f, 0.76f, 0.68f, 1.0f);
                break;
            default:
                spriteName = wildernessFurniture[furnitureIdx % wildernessFurniture.size()];
                tint = glm::vec4(0.78f, 0.76f, 0.72f, 0.95f);
                break;
            }

            spawnFurniture(tile.x, tile.y, spriteName, tint, 4);

            // Lightweight micro-encounter accents for visual storytelling.
            if ((furnitureIdx % 13) == 0)
            {
                const int nx = tile.x + (((furnitureIdx / 2) % 2 == 0) ? 1 : -1);
                const int ny = tile.y + (((furnitureIdx / 5) % 2 == 0) ? 0 : 1);
                if (!mTileMap.isSolid(nx, ny))
                {
                    const char* accentSprite = "env_crate";
                    if (dt == DistrictType::Commercial)
                    {
                        accentSprite = "env_barricade";
                    }
                    else if (dt == DistrictType::Industrial)
                    {
                        accentSprite = "env_noise_trap";
                    }
                    spawnFurniture(nx, ny, accentSprite, tint * glm::vec4(0.94f, 0.94f, 0.94f, 0.9f), 4);
                }
            }

            if ((furnitureIdx % 17) == 0)
            {
                const int sx = tile.x + (((furnitureIdx / 3) % 2 == 0) ? 0 : -1);
                const int sy = tile.y + (((furnitureIdx / 4) % 2 == 0) ? 1 : 0);
                if (!mTileMap.isSolid(sx, sy))
                {
                    const char* stain = (dt == DistrictType::Industrial) ? "env_blood_large" : "env_blood_small";
                    spawnFurniture(sx, sy, stain, glm::vec4(0.78f, 0.18f, 0.14f, 0.70f), 3);
                }
            }

            ++furnitureIdx;
        }
    }

    // --- Indoor decor (ambient clutter and distress cues inside buildings) ---
    {
        const std::array<const char*, 5> residentialIndoor = {
            "env_blood_small", "env_blood_large", "env_shelf", "env_crate", "env_sleeping_area"
        };
        const std::array<const char*, 5> commercialIndoor = {
            "env_shelf", "env_crate", "env_blood_small", "env_blood_large", "env_shelf"
        };
        const std::array<const char*, 5> industrialIndoor = {
            "env_crate", "env_noise_trap", "env_barricade", "env_blood_large", "env_shelf"
        };
        const std::array<const char*, 4> wildernessIndoor = {
            "env_crate", "env_blood_small", "env_blood_large", "env_shelf"
        };

        const auto spawnDecor = [&](int tileX, int tileY, const char* spriteName, const glm::vec4& color, int layer)
        {
            Entity decor = mWorld.createEntity();
            Transform t{};
            t.x = static_cast<float>(tileX * mTileMap.tileWidth());
            t.y = static_cast<float>(tileY * mTileMap.tileHeight());
            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect(spriteName);
            s.color = color;
            s.layer = layer;
            mWorld.addComponent<Transform>(decor, t);
            mWorld.addComponent<Sprite>(decor, s);
        };

        int decorIdx = 0;
        for (const auto& tile : mSpawnData.decorIndoorSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* spriteName = "env_crate";
            glm::vec4 tint = glm::vec4(0.9f, 0.85f, 0.8f, 1.0f);

            switch (dt)
            {
            case DistrictType::Residential:
                spriteName = residentialIndoor[decorIdx % residentialIndoor.size()];
                tint = glm::vec4(0.92f, 0.88f, 0.82f, 1.0f);
                break;
            case DistrictType::Commercial:
                spriteName = commercialIndoor[decorIdx % commercialIndoor.size()];
                tint = glm::vec4(0.86f, 0.88f, 0.90f, 1.0f);
                break;
            case DistrictType::Industrial:
                spriteName = industrialIndoor[decorIdx % industrialIndoor.size()];
                tint = glm::vec4(0.84f, 0.80f, 0.74f, 1.0f);
                break;
            default:
                spriteName = wildernessIndoor[decorIdx % wildernessIndoor.size()];
                tint = glm::vec4(0.80f, 0.78f, 0.74f, 0.95f);
                break;
            }

            spawnDecor(tile.x, tile.y, spriteName, tint, 4);

            if ((decorIdx % 9) == 0)
            {
                const int nx = tile.x + (((decorIdx / 3) % 2 == 0) ? 1 : -1);
                const int ny = tile.y + (((decorIdx / 2) % 2 == 0) ? 0 : 1);
                if (!mTileMap.isSolid(nx, ny))
                {
                    const char* clusterSprite = (dt == DistrictType::Industrial) ? "env_crate" : "env_shelf";
                    spawnDecor(nx, ny, clusterSprite, tint * glm::vec4(0.95f, 0.95f, 0.95f, 0.9f), 4);
                }
            }

            ++decorIdx;
        }
    }

    // --- Outdoor decor (debris, abandoned props) ---
    {
        const std::array<const char*, 5> residentialOutdoor = {
            "env_grave", "env_grave2", "env_campfire", "env_crate", "env_barricade"
        };
        const std::array<const char*, 5> commercialOutdoor = {
            "env_barricade", "env_noise_trap", "env_crate", "env_grave2", "env_campfire"
        };
        const std::array<const char*, 5> industrialOutdoor = {
            "env_noise_trap", "env_barricade", "env_crate", "env_campfire", "env_grave"
        };
        const std::array<const char*, 4> wildernessOutdoor = {
            "env_grave", "env_campfire", "env_crate", "env_grave2"
        };

        int decorIdx = 0;
        for (const auto& tile : mSpawnData.decorOutdoorSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* spriteName = "env_crate";
            glm::vec4 tint = glm::vec4(0.7f, 0.65f, 0.6f, 0.9f);

            switch (dt)
            {
            case DistrictType::Residential:
                spriteName = residentialOutdoor[decorIdx % residentialOutdoor.size()];
                tint = glm::vec4(0.74f, 0.69f, 0.63f, 0.9f);
                break;
            case DistrictType::Commercial:
                spriteName = commercialOutdoor[decorIdx % commercialOutdoor.size()];
                tint = glm::vec4(0.68f, 0.70f, 0.74f, 0.92f);
                break;
            case DistrictType::Industrial:
                spriteName = industrialOutdoor[decorIdx % industrialOutdoor.size()];
                tint = glm::vec4(0.72f, 0.66f, 0.58f, 0.92f);
                break;
            default:
                spriteName = wildernessOutdoor[decorIdx % wildernessOutdoor.size()];
                tint = glm::vec4(0.62f, 0.66f, 0.60f, 0.88f);
                break;
            }

            Entity decor = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(spriteName);
            s.color = tint; s.layer = 5;
            mWorld.addComponent<Transform>(decor, t);
            mWorld.addComponent<Sprite>(decor, s);

            if ((decorIdx % 11) == 0)
            {
                const int nx = tile.x + (((decorIdx / 2) % 2 == 0) ? -1 : 1);
                const int ny = tile.y + (((decorIdx / 4) % 2 == 0) ? 1 : 0);
                if (!mTileMap.isSolid(nx, ny))
                {
                    Entity clusterDecor = mWorld.createEntity();
                    Transform ct{};
                    ct.x = static_cast<float>(nx * mTileMap.tileWidth());
                    ct.y = static_cast<float>(ny * mTileMap.tileHeight());
                    Sprite cs{};
                    cs.textureId = sheetTex;
                    cs.uvRect = mSpriteSheet.uvRect("env_crate");
                    cs.color = tint * glm::vec4(0.95f, 0.95f, 0.95f, 0.85f);
                    cs.layer = 4;
                    mWorld.addComponent<Transform>(clusterDecor, ct);
                    mWorld.addComponent<Sprite>(clusterDecor, cs);
                }
            }

            ++decorIdx;
        }
    }

    // --- Blood splatters ---
    {
        static const char* kBloodSprites[] = {"env_blood_small", "env_blood_large"};
        int bloodIdx = 0;
        for (const auto& tile : mSpawnData.bloodSpawns)
        {
            if (mTileMap.isSolid(tile.x, tile.y)) continue;
            Entity blood = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(kBloodSprites[bloodIdx % 2]);
            s.color = glm::vec4(0.8f, 0.15f, 0.1f, 0.7f); s.layer = 3;
            mWorld.addComponent<Transform>(blood, t);
            mWorld.addComponent<Sprite>(blood, s);
            ++bloodIdx;
        }
    }

    // --- Burned tile marks restored from world memory ---
    {
        mBurnedTiles.clear();
        for (const auto& [tx, ty] : mSaveManager.burnedTiles())
        {
            if (tx < 0 || tx >= mTileMap.width() || ty < 0 || ty >= mTileMap.height())
            {
                continue;
            }

            addUniqueTilePair(mBurnedTiles, tx, ty);

            Entity scorch = mWorld.createEntity();
            Transform t{};
            t.x = static_cast<float>(tx * mTileMap.tileWidth());
            t.y = static_cast<float>(ty * mTileMap.tileHeight());

            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect("env_blood_large");
            s.color = glm::vec4(0.10f, 0.08f, 0.08f, 0.42f);
            s.layer = 2;

            mWorld.addComponent<Transform>(scorch, t);
            mWorld.addComponent<Sprite>(scorch, s);
        }
        mBurnedTilesAtRunStart = mBurnedTiles.size();
    }

    // --- Active fire tiles restored from world memory ---
    {
        for (const FireTileRecord& fireRecord : mSaveManager.fireTiles())
        {
            if (fireRecord.tileX < 0 || fireRecord.tileX >= mTileMap.width() ||
                fireRecord.tileY < 0 || fireRecord.tileY >= mTileMap.height())
            {
                continue;
            }

            if (mTileMap.isSolid(fireRecord.tileX, fireRecord.tileY))
            {
                continue;
            }

            Entity fireEntity = mWorld.createEntity();

            Transform t{};
            t.x = static_cast<float>(fireRecord.tileX * mTileMap.tileWidth());
            t.y = static_cast<float>(fireRecord.tileY * mTileMap.tileHeight());

            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect("env_campfire_lit");
            const float intensity = std::clamp(fireRecord.intensity, 0.0f, 2.0f);
            s.color = glm::vec4(1.0f, 0.45f + intensity * 0.20f, 0.18f, 0.55f + intensity * 0.18f);
            s.layer = 6;

            Fire fire{};
            fire.intensity = intensity;
            fire.lifeRemaining = std::max(2.0f, fireRecord.lifeRemaining);
            fire.spreadTimer = std::fmod(static_cast<float>(runRng() % 1000) * 0.001f, 1.0f);
            fire.noiseTimer = 0.0f;

            mWorld.addComponent<Transform>(fireEntity, t);
            mWorld.addComponent<Sprite>(fireEntity, s);
            mWorld.addComponent<Fire>(fireEntity, fire);
        }
    }

    // --- Sedan vehicles (M8 narrow slice) ---
    {
        auto spawnSedan = [&](int tileX, int tileY, float condition, float fuel, float maxFuel,
                              const std::vector<std::pair<int, int>>& items)
        {
            if (tileX < 0 || tileX >= mTileMap.width() || tileY < 0 || tileY >= mTileMap.height())
            {
                return;
            }
            if (mTileMap.isSolid(tileX, tileY))
            {
                return;
            }

            Entity vehicleEntity = mWorld.createEntity();

            Transform t{};
            t.x = static_cast<float>(tileX * mTileMap.tileWidth());
            t.y = static_cast<float>(tileY * mTileMap.tileHeight());

            Sprite s{};
            s.textureId = sheetTex;
            s.uvRect = mSpriteSheet.uvRect("env_cabinet");
            const float wearTint = std::clamp(condition / 100.0f, 0.0f, 1.0f);
            s.color = glm::vec4(0.33f + wearTint * 0.20f, 0.36f + wearTint * 0.17f, 0.40f + wearTint * 0.18f, 1.0f);
            s.layer = 5;

            Interactable inter{};
            inter.type = InteractableType::Container;
            inter.prompt = "Open trunk";

            Inventory trunk{};
            for (const auto& [itemId, count] : items)
            {
                if (itemId >= 0 && count > 0)
                {
                    InventorySystem::addItem(trunk, mItemDatabase, itemId, count);
                }
            }

            Vehicle vehicle{};
            vehicle.condition = std::clamp(condition, 0.0f, 100.0f);
            vehicle.maxFuel = std::max(1.0f, maxFuel);
            vehicle.fuel = std::clamp(fuel, 0.0f, vehicle.maxFuel);
            vehicle.engineOn = false;
            vehicle.playerInside = false;

            mWorld.addComponent<Transform>(vehicleEntity, t);
            mWorld.addComponent<Sprite>(vehicleEntity, s);
            mWorld.addComponent<Interactable>(vehicleEntity, inter);
            mWorld.addComponent<Inventory>(vehicleEntity, trunk);
            mWorld.addComponent<Vehicle>(vehicleEntity, vehicle);
        };

        const auto& persistedVehicles = mSaveManager.vehicles();
        if (!persistedVehicles.empty())
        {
            for (const auto& record : persistedVehicles)
            {
                const int tileX = static_cast<int>(std::floor(record.x / static_cast<float>(std::max(1, mTileMap.tileWidth()))));
                const int tileY = static_cast<int>(std::floor(record.y / static_cast<float>(std::max(1, mTileMap.tileHeight()))));

                // Mild between-run wear to communicate world persistence pressure.
                const float carryCondition = std::max(0.0f, record.condition - 4.0f);
                spawnSedan(tileX, tileY, carryCondition, record.fuel, record.maxFuel, record.items);
            }
        }
        else
        {
            std::uniform_real_distribution<float> fuelDist(8.0f, 24.0f);
            std::uniform_real_distribution<float> conditionDist(45.0f, 95.0f);
            std::uniform_int_distribution<int> trunkRoll(0, 100);

            for (const auto& tile : mSpawnData.vehicleSpawns)
            {
                std::vector<std::pair<int, int>> trunkItems;
                if (trunkRoll(runRng) < 45) trunkItems.emplace_back(12, 1); // fuel can
                if (trunkRoll(runRng) < 40) trunkItems.emplace_back(3, 1);  // water bottle
                if (trunkRoll(runRng) < 30) trunkItems.emplace_back(4, 1);  // bandage

                spawnSedan(
                    tile.x,
                    tile.y,
                    conditionDist(runRng),
                    fuelDist(runRng),
                    30.0f,
                    trunkItems);
            }
        }
    }

    // --- Loot containers (district-aware loot profiles) ---
    {
        const int itemDbSize = static_cast<int>(mItemDatabase.allItems().size());
        std::uniform_int_distribution<int> lootCountDist(2, 4);
        std::uniform_int_distribution<int> lootQtyDist(1, 3);

        // District loot tables (item IDs)
        // Residential: food(0,1,2,21), drink(3), jacket(20), flashlight(18)
        // Commercial: weapons(5-8,17), medical(4,15,16,22), general
        // Industrial: materials(9-14,18-20), bandages(4), tools
        const std::array<int, 9> residentialLoot = {0, 1, 2, 21, 3, 3, 23, 20, 18};
        const std::array<int, 7> commercialLoot  = {5, 6, 7, 8, 17, 22, 23};
        const std::array<int, 6> industrialLoot  = {9, 10, 11, 12, 18, 4};

        const auto isSpecialCacheTile = [&](const glm::ivec2& tile)
        {
            const auto matchesTile = [&](const glm::ivec2& other)
            {
                return other.x == tile.x && other.y == tile.y;
            };

            return std::any_of(mSpawnData.medicalCacheSpawns.begin(), mSpawnData.medicalCacheSpawns.end(), matchesTile) ||
                   std::any_of(mSpawnData.supplyCacheSpawns.begin(), mSpawnData.supplyCacheSpawns.end(), matchesTile);
        };

        // Build set of previously looted tile positions for fast lookup
        const auto& lootedPos = mSaveManager.lootedPositions();
        auto isLooted = [&](int tx, int ty) -> bool
        {
            for (const auto& [lx, ly] : lootedPos)
            {
                if (lx == tx && ly == ty) return true;
            }
            return false;
        };

        for (const auto& tile : mSpawnData.containerSpawns)
        {
            if (isSpecialCacheTile(tile)) continue;
            if (mTileMap.isSolid(tile.x, tile.y)) continue;

            // Skip containers at previously looted positions
            if (isLooted(tile.x, tile.y)) continue;

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* containerSprite = "env_crate";
            glm::vec4 containerTint = glm::vec4(0.8f, 0.7f, 0.5f, 1.0f);
            const char* containerPrompt = "Search stash";

            switch (dt)
            {
            case DistrictType::Residential:
                containerSprite = "env_shelf";
                containerTint = glm::vec4(0.83f, 0.79f, 0.72f, 1.0f);
                containerPrompt = "Search pantry";
                break;
            case DistrictType::Commercial:
                containerSprite = "env_crate";
                containerTint = glm::vec4(0.72f, 0.76f, 0.80f, 1.0f);
                containerPrompt = "Search storefront";
                break;
            case DistrictType::Industrial:
                containerSprite = "env_crate";
                containerTint = glm::vec4(0.80f, 0.72f, 0.58f, 1.0f);
                containerPrompt = "Search workshop crate";
                break;
            default:
                containerSprite = "env_crate";
                containerTint = glm::vec4(0.72f, 0.69f, 0.62f, 1.0f);
                containerPrompt = "Search abandoned stash";
                break;
            }

            Entity container = mWorld.createEntity();
            Transform t{}; t.x = static_cast<float>(tile.x * mTileMap.tileWidth()); t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
            Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(containerSprite); s.color = containerTint; s.layer = 5;
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = containerPrompt;
            Inventory containerInv{};
            mWorld.addComponent<Transform>(container, t);
            mWorld.addComponent<Sprite>(container, s);
            mWorld.addComponent<Interactable>(container, inter);
            mWorld.addComponent<Inventory>(container, containerInv);
            Inventory& inv = mWorld.getComponent<Inventory>(container);

            const int lootTypes = lootCountDist(runRng);
            for (int li = 0; li < lootTypes; ++li)
            {
                int itemId;
                switch (dt)
                {
                case DistrictType::Residential:
                    itemId = residentialLoot[runRng() % residentialLoot.size()];
                    break;
                case DistrictType::Commercial:
                    itemId = commercialLoot[runRng() % commercialLoot.size()];
                    break;
                case DistrictType::Industrial:
                    itemId = industrialLoot[runRng() % industrialLoot.size()];
                    break;
                default: // Wilderness — random from full database
                    itemId = std::uniform_int_distribution<int>(0, std::max(0, itemDbSize - 1))(runRng);
                    break;
                }
                InventorySystem::addItem(inv, mItemDatabase, itemId, lootQtyDist(runRng));
            }
        }
    }

    // --- Ambient journal notes (district-authored flavor lines) ---
    {
        const std::array<const char*, 6> residentialNotes = {
            "The upstairs door sticks; kick low, not high.",
            "Left canned food under the sink. If it's gone, so am I.",
            "We marked each room before dark. Do the same.",
            "Our block looked safe until the sirens stopped.",
            "If you hear a baby monitor, leave immediately.",
            "Back fence has one loose board for escape.",
        };
        const std::array<const char*, 6> commercialNotes = {
            "Checkout lane still has meds taped underneath.",
            "The alarms died, but they still come to broken glass.",
            "Office stairwell is clear until sunset.",
            "Security shutters jam at the halfway point.",
            "Storage room had water on day four. Not on day seven.",
            "If the storefront is quiet, check the loading bay first.",
        };
        const std::array<const char*, 6> industrialNotes = {
            "North warehouse has tools, south warehouse has teeth.",
            "We welded one gate shut and regretted it.",
            "Forklift battery still works for floodlights.",
            "Machine room echoes hide footsteps until they're close.",
            "Look for painted arrows near the tanks.",
            "Keep one wrench. Doors fail more than people.",
        };
        const std::array<const char*, 5> wildernessNotes = {
            "Campfire ash is fresh. Stay alert.",
            "Water here tastes metallic after rain.",
            "Tracks circle this lot every dawn.",
            "We buried supplies under the fallen sign.",
            "If birds go silent, move now.",
        };

        const auto spawnAmbientNote = [&](int tileX, int tileY, const std::string& text)
        {
            Entity noteEntity = mWorld.createEntity();

            Transform nt{};
            nt.x = static_cast<float>(tileX * mTileMap.tileWidth()) + 5.0f;
            nt.y = static_cast<float>(tileY * mTileMap.tileHeight()) + 4.0f;

            Sprite ns{};
            ns.textureId = sheetTex;
            ns.uvRect = mSpriteSheet.uvRect("item_bandage");
            ns.color = glm::vec4(0.84f, 0.79f, 0.62f, 0.95f);
            ns.layer = 6;
            ns.drawSize = SpriteSizes::kPickup;

            Interactable ni{};
            ni.type = InteractableType::JournalNote;
            ni.prompt = "Read note";
            ni.message = text;

            mWorld.addComponent<Transform>(noteEntity, nt);
            mWorld.addComponent<Sprite>(noteEntity, ns);
            mWorld.addComponent<Interactable>(noteEntity, ni);
        };

        int notesPlaced = 0;
        const int noteLimit = std::max(6, std::min(14, static_cast<int>(mSpawnData.decorOutdoorSpawns.size() / 6 + mSpawnData.decorIndoorSpawns.size() / 10)));
        int noteSeed = 0;

        for (const auto& tile : mSpawnData.decorOutdoorSpawns)
        {
            if (notesPlaced >= noteLimit) break;
            if ((noteSeed % 7) != 0)
            {
                ++noteSeed;
                continue;
            }
            if (mTileMap.isSolid(tile.x, tile.y))
            {
                ++noteSeed;
                continue;
            }

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* line = nullptr;
            switch (dt)
            {
            case DistrictType::Residential:
                line = residentialNotes[(noteSeed + notesPlaced) % residentialNotes.size()];
                break;
            case DistrictType::Commercial:
                line = commercialNotes[(noteSeed + notesPlaced) % commercialNotes.size()];
                break;
            case DistrictType::Industrial:
                line = industrialNotes[(noteSeed + notesPlaced) % industrialNotes.size()];
                break;
            default:
                line = wildernessNotes[(noteSeed + notesPlaced) % wildernessNotes.size()];
                break;
            }

            spawnAmbientNote(tile.x, tile.y, std::string("Scrawled note: ") + line);
            ++notesPlaced;
            ++noteSeed;
        }

        for (const auto& tile : mSpawnData.decorIndoorSpawns)
        {
            if (notesPlaced >= noteLimit) break;
            if ((noteSeed % 11) != 0)
            {
                ++noteSeed;
                continue;
            }
            if (mTileMap.isSolid(tile.x, tile.y))
            {
                ++noteSeed;
                continue;
            }

            const DistrictType dt = getDistrictType(tile.x, tile.y);
            const char* line = nullptr;
            switch (dt)
            {
            case DistrictType::Residential:
                line = residentialNotes[(noteSeed + notesPlaced) % residentialNotes.size()];
                break;
            case DistrictType::Commercial:
                line = commercialNotes[(noteSeed + notesPlaced) % commercialNotes.size()];
                break;
            case DistrictType::Industrial:
                line = industrialNotes[(noteSeed + notesPlaced) % industrialNotes.size()];
                break;
            default:
                line = wildernessNotes[(noteSeed + notesPlaced) % wildernessNotes.size()];
                break;
            }

            spawnAmbientNote(tile.x, tile.y, std::string("Journal scrap: ") + line);
            ++notesPlaced;
            ++noteSeed;
        }
    }

    // Restore persisted structures (degraded between runs)
    for (const StructureRecord& rec : mSaveManager.structures())
    {
        const int recipeIdx = rec.type;
        if (recipeIdx < 0 || recipeIdx >= BuildSystem::kRecipeCount) continue;
        const auto& recipe = BuildSystem::kRecipes[recipeIdx];

        Entity structEntity = mWorld.createEntity();
        Transform t{}; t.x = rec.x; t.y = rec.y;
        Sprite s{}; s.textureId = sheetTex; s.uvRect = mSpriteSheet.uvRect(recipe.sprite); s.color = glm::vec4(1.0f); s.layer = 4;
        Structure structure{}; structure.type = recipe.type; structure.maxHp = recipe.hp;
        structure.hp = recipe.hp * rec.hpRatio;
        structure.condition = static_cast<StructureCondition>(rec.condition);
        mWorld.addComponent<Transform>(structEntity, t);
        mWorld.addComponent<Sprite>(structEntity, s);
        mWorld.addComponent<Structure>(structEntity, structure);

        // Restore Supply Cache inventory
        if (recipe.type == StructureType::SupplyCache && !rec.items.empty())
        {
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Loot cache";
            mWorld.addComponent<Interactable>(structEntity, inter);
            Inventory inv{};
            for (const auto& [itemId, count] : rec.items)
            {
                InventorySystem::addItem(inv, mItemDatabase, itemId, count);
            }
            mWorld.addComponent<Inventory>(structEntity, inv);
        }
        // Rain Collector gets container but no items (regenerates fresh)
        else if (recipe.type == StructureType::RainCollector)
        {
            Interactable inter{}; inter.type = InteractableType::Container; inter.prompt = "Collect water";
            mWorld.addComponent<Interactable>(structEntity, inter);
            mWorld.addComponent<Inventory>(structEntity, Inventory{});
        }
    }

    const auto spawnGraveMarker = [&](const GraveRecord& grave)
    {
        Entity graveEntity = mWorld.createEntity();

        Transform graveTransform{};
        graveTransform.x = grave.x;
        graveTransform.y = grave.y;

        Sprite graveSprite{};
        graveSprite.textureId = sheetTex;
        graveSprite.uvRect = mSpriteSheet.uvRect("env_grave");
        graveSprite.color = glm::vec4(1.0f);
        graveSprite.layer = 7;

        Interactable graveInteract{};
        graveInteract.type = InteractableType::GraveMarker;
        graveInteract.prompt = "Read epitaph";

        const int graveTileX = static_cast<int>(grave.x) / std::max(1, mTileMap.tileWidth());
        const int graveTileY = static_cast<int>(grave.y) / std::max(1, mTileMap.tileHeight());
        const DistrictType graveDistrict = getDistrictType(graveTileX, graveTileY);
        const char* districtLabel = "WILDERNESS";
        switch (graveDistrict)
        {
        case DistrictType::Residential: districtLabel = "RESIDENTIAL"; break;
        case DistrictType::Commercial: districtLabel = "COMMERCIAL"; break;
        case DistrictType::Industrial: districtLabel = "INDUSTRIAL"; break;
        default: districtLabel = "WILDERNESS"; break;
        }

        std::ostringstream message;
        message << "Here lies " << grave.name << " | Survived " << grave.day
            << " days | " << districtLabel << " | " << grave.cause;
        graveInteract.message = message.str();

        mWorld.addComponent<Transform>(graveEntity, graveTransform);
        mWorld.addComponent<Sprite>(graveEntity, graveSprite);
        mWorld.addComponent<Interactable>(graveEntity, graveInteract);

        // Spawn journal note beside the grave
        {
            Entity journalEntity = mWorld.createEntity();

            Transform jt{};
            jt.x = grave.x + 20.0f;
            jt.y = grave.y + 8.0f;

            Sprite js{};
            js.textureId = sheetTex;
            js.uvRect = mSpriteSheet.uvRect("item_bandage"); // reuse small item sprite
            js.color = glm::vec4(0.85f, 0.80f, 0.65f, 1.0f); // parchment tint
            js.layer = 6;
            js.drawSize = SpriteSizes::kPickup;

            // Generate procedural flavor text from the grave data
            const std::array<const char*, 5> residentialFlavor = {
                "The house alarms died, but the footsteps didn't.",
                "I barricaded the kitchen and still heard them upstairs.",
                "Family photos stayed on the wall longer than we did.",
                "Backyards connect faster than streets after dark.",
                "Every porch light looked like a signal.",
            };
            const std::array<const char*, 5> commercialFlavor = {
                "I slept in the stockroom between shipping pallets.",
                "The register drawers were empty before the shelves were.",
                "Reflections in storefront glass nearly got me killed.",
                "Loading docks are exits if you keep them clear.",
                "The mall speakers played static all night.",
            };
            const std::array<const char*, 5> industrialFlavor = {
                "The plant gates held until someone cut power.",
                "Tools mattered more than ammo by day three.",
                "Conveyors jammed with things that used to be people.",
                "The siren tower became a beacon for the dead.",
                "I trusted steel walls and forgot about roof access.",
            };
            const std::array<const char*, 4> wildernessFlavor = {
                "Open ground helps until exhaustion makes you loud.",
                "We kept moving camp but not fast enough.",
                "The quiet lots are where hope goes to thin out.",
                "Smoke carries farther than footsteps.",
            };

            const std::uint32_t flavorHash =
                (static_cast<std::uint32_t>(grave.day) * 2654435761u) ^
                static_cast<std::uint32_t>(grave.x + grave.y * 137.0f);

            const auto pickFlavor = [&](const auto& lines) -> const char*
            {
                const std::size_t idx = static_cast<std::size_t>(flavorHash % static_cast<std::uint32_t>(lines.size()));
                return lines[idx];
            };

            const char* flavorLine = nullptr;
            switch (graveDistrict)
            {
            case DistrictType::Residential: flavorLine = pickFlavor(residentialFlavor); break;
            case DistrictType::Commercial: flavorLine = pickFlavor(commercialFlavor); break;
            case DistrictType::Industrial: flavorLine = pickFlavor(industrialFlavor); break;
            default: flavorLine = pickFlavor(wildernessFlavor); break;
            }

            std::string causeHint;
            if (grave.cause.find("bite") != std::string::npos || grave.cause.find("Bite") != std::string::npos)
            {
                causeHint = "The bite went hot before dawn.";
            }
            else if (grave.cause.find("bleed") != std::string::npos || grave.cause.find("Bleed") != std::string::npos)
            {
                causeHint = "I ran out of clean wraps.";
            }
            else if (grave.cause.find("starv") != std::string::npos || grave.cause.find("Starv") != std::string::npos)
            {
                causeHint = "I traded too much food for one more day.";
            }
            else if (grave.cause.find("thirst") != std::string::npos || grave.cause.find("Thirst") != std::string::npos)
            {
                causeHint = "Dry taps ended us faster than the dead.";
            }
            else if (grave.cause.find("infection") != std::string::npos || grave.cause.find("Infection") != std::string::npos)
            {
                causeHint = "The fever came back worse each night.";
            }

            std::ostringstream journalMsg;
            journalMsg << grave.name << "'s journal (Day " << grave.day << "): "
                       << flavorLine;
            if (!causeHint.empty())
            {
                journalMsg << ' ' << causeHint;
            }

            Interactable ji{};
            ji.type = InteractableType::JournalNote;
            ji.prompt = "Read note";
            ji.message = journalMsg.str();

            mWorld.addComponent<Transform>(journalEntity, jt);
            mWorld.addComponent<Sprite>(journalEntity, js);
            mWorld.addComponent<Interactable>(journalEntity, ji);
        }
    };

    for (const GraveRecord& grave : mSaveManager.graves())
    {
        spawnGraveMarker(grave);
    }

    // Spawn retirement markers (visually distinct from graves)
    for (const RetirementRecord& ret : mSaveManager.retirements())
    {
        Entity retEntity = mWorld.createEntity();

        Transform retTransform{};
        retTransform.x = ret.x;
        retTransform.y = ret.y;

        Sprite retSprite{};
        retSprite.textureId = sheetTex;
        retSprite.uvRect = mSpriteSheet.uvRect("env_grave2");
        retSprite.color = glm::vec4(0.6f, 0.7f, 0.9f, 1.0f); // blue tint for retirement
        retSprite.layer = 7;

        Interactable retInteract{};
        retInteract.type = InteractableType::RetirementMarker;
        retInteract.prompt = "Read";

        std::ostringstream message;
        message << ret.name << " walked away on Day " << ret.day << " | They couldn't go on.";
        retInteract.message = message.str();

        mWorld.addComponent<Transform>(retEntity, retTransform);
        mWorld.addComponent<Sprite>(retEntity, retSprite);
        mWorld.addComponent<Interactable>(retEntity, retInteract);
    }

    const Transform& playerTransformRef = mWorld.getComponent<Transform>(mPlayerEntity);
    const glm::vec2 playerCenter(
        playerTransformRef.x + kSpriteSize * 0.5f,
        playerTransformRef.y + kSpriteSize * 0.5f);
    mCamera.snapTo(playerCenter);
    mCamera.clampToBounds(mTileMap.pixelSize());

    mRunStats.prevX = playerTransformRef.x;
    mRunStats.prevY = playerTransformRef.y;

    // Initialize prev positions for interpolation
    snapshotPositions();
}
