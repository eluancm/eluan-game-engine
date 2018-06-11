/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Particle management

All particle effects should be short in duration. Do not ever spawn a
particle effect in the start of a server that should last for the entire
server time, because a saved state is server-only and loading it will
have a brand new client, without the particles!

TODO: do not draw particles too far away (different zfar per effect size)
TODO: client-side entities/particles as baseline/static entities to
allow constant particle effects without having to resend a SVC

============================================================================
*/

#define MAX_PARTICLES			16384

/* these are the headers, they are NOT used as particles */
particle_t		*free_particles;
particle_t		*used_particles;

/*
===================
CL_CleanParticles
===================
*/
void CL_CleanParticles(void)
{
	particle_t *tmp;

	while (used_particles->next) /* clean one by one */
	{
		tmp = used_particles->next; /* save it */
		used_particles->next = used_particles->next->next; /* remove it from the used list */
		tmp->next = free_particles->next; /* put the free particles list after this one */
		free_particles->next = tmp; /* and set it as the start of the free particles list */
	}
}

/*
===================
CL_UpdateParticles

Should be called at the start of each frame to update particle position/velocity
===================
*/
void CL_UpdateParticles(void)
{
	particle_t *cur = used_particles;
	particle_t *tmp;

	/* we use headers to simplify the algorithms, then we are always checking "the next one" instead of the current */
	while (cur->next) /* while there are used particles after the header */
	{
		if (cur->next->timelimit <= cls.time) /* should disappear now */
		{
			tmp = cur->next; /* save it */
			cur->next = cur->next->next; /* remove it from the used list */
			tmp->next = free_particles->next; /* put the free particles list after this one */
			free_particles->next = tmp; /* and set it as the start of the free particles list */
			continue; /* now check the new cur->next to see if it should disappear */
		}

		/* TODO: if doing physics by hand, do not forget to integrate acceleration rightly */
		/* update origins */
		cur->next->org[0] += cur->next->vel[0] * ((vec_t)host_frametime / 1000.f);
		cur->next->org[1] += cur->next->vel[1] * ((vec_t)host_frametime / 1000.f);
		cur->next->org[2] += cur->next->vel[2] * ((vec_t)host_frametime / 1000.f);

		cur = cur->next; /* check the next one */
	}
}

/*
===================
CL_ParticleAlloc

Returns a particle from the free particle pool and puts it in the used particle pool
===================
*/
particle_t *CL_ParticleAlloc(void)
{
	particle_t *newpart;

	if (free_particles->next)
	{
		/* remove from one */
		newpart = free_particles->next;
		free_particles->next = free_particles->next->next;

		/* and put into the other */
		newpart->next = used_particles->next;
		used_particles->next = newpart;

		return newpart;
	}
	else
	{
		Sys_Printf("CL_ParticleAlloc: no free particles\n");
		return NULL; /* DO NOT ERROR if there are no free particles */
	}
}

/*
===================
CL_StartParticle

Starts a particle effect
===================
*/
void CL_StartParticle(char *msg, int *read, int len)
{
	Game_CL_StartParticle(msg, read, len);
}

/*
===================
CL_ParticlesInit

Should be called only once per session, creates the particle lists
===================
*/
void CL_ParticlesInit(void)
{
	int i;
	particle_t *new_part;

	/* these are the headers, they are NOT used as particles */
	used_particles = Sys_MemAlloc(&std_mem, sizeof(particle_t), "particle");;
	free_particles = Sys_MemAlloc(&std_mem, sizeof(particle_t), "particle");

	used_particles->next = NULL;
	free_particles->next = NULL;
	new_part = free_particles;

	for (i = 0; i < MAX_PARTICLES; i++)
	{
		new_part->next = Sys_MemAlloc(&std_mem, sizeof(particle_t), "particle");
		new_part = new_part->next;
	}

	new_part->next = NULL; /* finish the list */
}

/*
===================
CL_ParticlesShutdown

Should be called only once per session
===================
*/
void CL_ParticlesShutdown(void)
{
	free_particles = NULL;
	used_particles = NULL;
}