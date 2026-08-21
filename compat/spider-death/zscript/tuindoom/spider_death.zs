// Lightweight Spider Mastermind death for the RT renderer.
// The reduced SPID model reuses the pain mesh for J-S, while this actor
// supplies a dense, randomized cosmetic explosion sequence.
class TuinSpiderMastermind : SpiderMastermind replaces SpiderMastermind
{
	void A_TuinSpiderExplosionBurst()
	{
		int count = random[TuinSpiderBurstCount](3, 7);

		for (int i = 0; i < count; i++)
		{
			Vector3 boomPos = Vec2OffsetZ(
				random[TuinSpiderBurstX](-104, 104),
				random[TuinSpiderBurstY](-104, 104),
				random[TuinSpiderBurstZ](8, 96));

			Actor boom = Actor.Spawn("Rocket", boomPos, NO_REPLACE);
			if (boom)
			{
				boom.DeathSound = "";
				boom.Vel.z = random[TuinSpiderBurstVelocity](-16, 48) / 16.0;
				boom.SetStateLabel("BrainExplode");
				boom.bRocketTrail = false;
				boom.SetDamage(0);
				boom.tics = random[TuinSpiderBurstFrame](1, 5);
			}
		}

		// Keep the bursts irregular and fast. The normal Spider death scream is
		// played separately at the start of the sequence.
		if (random[TuinSpiderBurstSound](0, 255) < 208)
		{
			A_StartSound("weapons/rocklx", CHAN_ITEM, CHANF_DEFAULT,
				random[TuinSpiderBurstVolume](70, 100) / 100.0, ATTN_IDLE);
		}

		tics = random[TuinSpiderBurstDelay](1, 3);
	}

	void A_TuinSpiderDeathStart()
	{
		A_Scream();
		A_TuinSpiderExplosionBurst();
	}

	void A_TuinSpiderDeathDrop()
	{
		A_NoBlocking();
		A_TuinSpiderExplosionBurst();
	}

	States
	{
	Death:
		SPID J 1 A_TuinSpiderDeathStart;
		SPID K 1 A_TuinSpiderDeathDrop;
		SPID L 1 A_TuinSpiderExplosionBurst;
		SPID M 1 A_TuinSpiderExplosionBurst;
		SPID N 1 A_TuinSpiderExplosionBurst;
		SPID O 1 A_TuinSpiderExplosionBurst;
		SPID P 1 A_TuinSpiderExplosionBurst;
		SPID Q 1 A_TuinSpiderExplosionBurst;
		SPID R 1 A_TuinSpiderExplosionBurst;
		SPID S 1 A_TuinSpiderExplosionBurst;
		SPID S 1 A_TuinSpiderExplosionBurst;
		SPID S 1 A_TuinSpiderExplosionBurst;
		SPID S 1 A_TuinSpiderExplosionBurst;
		SPID S 1 A_TuinSpiderExplosionBurst;
		SPID S 8;
		// Death frames reuse the intact pain mesh to keep the RT model small.
		// Fire the map's boss-death special while hidden, then remove that mesh
		// instead of leaving an apparently living Mastermind in the level.
		TNT1 A 0 A_BossDeath;
		Stop;
	}
}
