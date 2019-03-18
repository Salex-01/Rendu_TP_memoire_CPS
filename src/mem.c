#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#define LIMITER_ACCES_IMPREVUS 1

// constante définie dans gcc seulement
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif

#define LIMITER_ACCES_IMPREVUS 1

void* debut_mem;
int taille_mem;

typedef struct fb {
	size_t size;	//Taille de la partie Data (inclue next)
	struct fb* next;
}zonevide;

typedef struct ob {
	size_t size;
}zoneocc;


void mem_init(void* mem, size_t taille){
	assert(mem == get_memory_adr());
	assert(taille == get_memory_size());
	if(taille<ALIGNMENT+sizeof(zonevide*)){
		printf("Zone mémoire trop courte pour être initialisée\n");
		return;
	}
	*((zonevide**)(mem)) = (zonevide*)(mem+ALIGNMENT-sizeof(size_t));	// Adresse de la première zone vide
	*((size_t*)(mem+ALIGNMENT-sizeof(size_t))) = taille-ALIGNMENT;	// Taille de la première zone vide
	*((zonevide**)(mem+ALIGNMENT)) = (zonevide*)(NULL);	// Adresse de la zone vide suivante (qui n'existe pas encore)
	debut_mem = mem;
	taille_mem = taille;
	mem_fit(&mem_fit_first);
}

void mem_show(void (*print)(void *, size_t, int)) {
	void* p = debut_mem+ALIGNMENT-sizeof(size_t);
	zonevide* zv = *((zonevide**)(debut_mem));	//Prochaine zone vide qu'on rencontrera
	while(p<debut_mem+taille_mem){	//Tant qu'on est sur la bonne plage mémoire
		if(p==zv){	//Si c'est une zone vide
			print(p+sizeof(size_t), zv->size, 1);	//Afficher zone vide
			p += (zv->size + ALIGNMENT);	//Accès à la zone suivante
			zv = zv->next;	//Calcul de la prochaine zone vide qu'on rencontrera
		}
		else{
			if(p==debut_mem+ALIGNMENT-sizeof(size_t)){
				print(p+sizeof(size_t), ((zoneocc*)p)->size, 0);	// La première zone occupée ne dispose pas de champs "taille demandée par l'utilisateur" par manque de place
			}
			else{
				print(p+sizeof(size_t), *((unsigned int*)(p-sizeof(int))), 0);	//Afficher zone occupée
			}
			p += (((zoneocc*)p)->size + ALIGNMENT);	//Accès à la zone suivante
		}
	}
}

static mem_fit_function_t *mem_fit_fn;
void mem_fit(mem_fit_function_t *f) {
	mem_fit_fn=f;
}

void retirerzv(zonevide* zv){
	zonevide* p = *((zonevide**)debut_mem);	//Première zone vide
	if(p==zv){	//Si on veut retirer la première zone vide
		*((zonevide**)debut_mem) = zv->next;	//On remplace simplement la première zone vide par la suivante
	}
	else{	//Sinon
		while((p!=NULL)&&(p->next!=zv)){	//On avance jusqu'à la zone vide qui précède celle à supprimer
			p = p->next;
		}
		if(p==NULL){	//Si on a pas trouvé la zone à supprimer
			return;
		}
		p->next = p->next->next;	//On remplace la zone à supprimer par la suivante
	}
}

void *mem_alloc(size_t taille) {
	__attribute__((unused)) // juste pour que gcc compile ce squelette avec -Werror
	zonevide* zv = mem_fit_fn(debut_mem, taille);	//On cherche une zone vide assez grande
	if(zv==NULL){	//Si on n'en a pas trouvé
		return NULL;
	}
	int lpad = ALIGNMENT-taille%ALIGNMENT;	// Longueur du padding
	if(lpad==ALIGNMENT){	//Pour ne pas perdre ALIGNMENT octets quand on alloue n*ALIGNMENT octets
		lpad = 0;
	}
	if(zv->size!=taille){	//Si on ne remplace pas complètement la zone vide
		zv->size -= ALIGNMENT;	//Il faudra laisser un ALIGNMENT pour stocker les metadonnées de la zone occupée
	}
	zv->size -= (taille + lpad);	//Et raccourcir la zone vide de la longueur des données et du padding de la zone occupée
	long int tmpadd = (long int)zv;	//Adresse de la zone occupée
	if(zv->size != 0){	//Si on ne remplace pas la zone vide
		tmpadd += zv->size + ALIGNMENT;	//On avance jusqu'à l'emplacement réel de la zone occupée
	}
	zoneocc* o = (zoneocc*)tmpadd;
	if(zv->size==0){
		retirerzv(zv);	//Si on remplace totalement la zone vide
	}
	o->size = taille+lpad;	//Taille de la zone occupée (incluant le padding)
	if((void*)o>=(get_memory_adr() + 2*sizeof(size_t) + sizeof(void*))){
		*((size_t*)(((long int)o)-sizeof(size_t))) = taille;	//On stocke aussi sa taille réelle dans un int juste avant la taille totale
									//Sauf si cela ecraserait le pointeur sur la première zone vide (le premier emplacement de la mémoire)
	}
	return ((void*)o)+sizeof(size_t);	//On retourne l'adresse du début de la zone data
}

void compresser(zonevide* zv){	//Pour fuionner des zones vides consécutives (Faites un schéma)
	if(zv->next->next == (zonevide*)(((long int)(zv->next)) + zv->next->size + ALIGNMENT)){	//Si la zone suivant la prochaine zone vide est une zone vide
		zv->next->size += zv->next->next->size + ALIGNMENT;	//On augmente ajoute la taille de la zone vide suivante
		zv->next->next = zv->next->next->next;	//On remplace la zone par la suivante-suivante par la zone vide qui suit
	}
	if(zv->next == (zonevide*)(((long int)(zv)) + zv->size + ALIGNMENT)){	//On applique le même traitement pour les 2 premières zones
		zv->size += zv->next->size + ALIGNMENT;
		zv->next = zv->next->next;
	}
}

void mem_free(void* mem) {
	zonevide* p = (zonevide*)(mem-sizeof(size_t));
	zonevide* zv = *((zonevide**)(debut_mem));	//Adresse de la première zone vide
	if((zv!=NULL)&&(zv<p)){	//Si on doit insérer la nouvelle zone vide après la première de la liste
		while((zv->next!=NULL)&&(zv->next<p)){	//On recherche l'emplacement où elle doit aller
			zv = zv->next;
		}
		p->next = zv->next;	//On l'insère
		zv->next = p;
		compresser(zv);	//On compresse les zones vides consécutives en une seule
	}
	else{	//Sinon
		p->next = *(zonevide**)debut_mem;	//On l'insère au début de la liste
		*(zonevide**)debut_mem = p;
		if(p->next!=NULL){
			compresser(p);	//Et on compresse les zones vides si il y en a plusieurs
		}
	}
}


struct fb* mem_fit_first(struct fb *list, size_t size) {
	zonevide* p = *((zonevide**)debut_mem);
	while(p!=NULL){	//On parcourt simplement les zones vides jusqu'à en trouver une qui convient
		if(p->size >= size){
			return p;
		}
		p = p->next;
	}
	return NULL;
}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	if(LIMITER_ACCES_IMPREVUS){
		//Retourne la taille que l'utilisateur avait demandé d'allouer
		zone -= (2*sizeof(size_t));
		return *((size_t*)zone);
	}
	else{
		//Retourne la taille maximum utilisable sans corrompre la mémoire
		zone -= sizeof(size_t);
		return ((zoneocc*)zone)->size;
	}
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	return NULL;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	return NULL;
}
