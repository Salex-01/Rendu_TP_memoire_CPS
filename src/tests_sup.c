#include <stdio.h>
#include "mem.h"
#include "common.h"
#include "time.h"

#define NZONES 30

int main(){
	mem_init(get_memory_adr(), get_memory_size());
	
	printf("\nAllocation d'une zone de taille max (127984)\n");
	mem_alloc(127984);
	printf("Libération de la zone\n");
	mem_free(get_memory_adr() + 16);

	printf("Allocation de %d zones de taille 16\n",NZONES);
	for(int i = 0; i<NZONES; i++){
		mem_alloc(16);
	}
	printf("Libérationn des zones\n");
	for(int i = 127984; i>127984-NZONES*32; i-=32){
		mem_free(get_memory_adr() + i);
	}

	printf("Allocation de %d zones de taille aléatoire\n",NZONES);
	for(int i = 0; i<NZONES; i++){
		mem_alloc((rand())%100);
	}

	printf("Réinitialisation de la mémoire\n");
	mem_init(get_memory_adr(), get_memory_size());

	printf("Test de fusion des zones libres consécutives\n");
	mem_alloc(16);
	mem_alloc(16);
	mem_alloc(16);
	mem_alloc(16);
	printf("Libération de la première et des 2 dernières zones occupées\n");
	mem_free(get_memory_adr() + 127984);
	mem_free(get_memory_adr() + 127952);
	mem_free(get_memory_adr() + 127888);
}
