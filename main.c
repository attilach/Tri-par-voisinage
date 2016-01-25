/**
* @fichier tri.c
* @titre Tri par voisinage
* @description Tri à bulles (bubblesort) réparti entre plusieurs threads
* @auteurs Kevin Estalella
* @date 10 janvier 2016
* @version 1.0
*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

//Valeur maximale que peut contenir une case du tableau
const int nbr_maximum = 55;

int nbr_attente = 0;	//Nombre de threads en attente

int *tableau;	//Tableau qui va contenir nos nombres
int tab_size;	//Taille du tableau

int nbr_zones;	//Nombre de zones du tableau

//Structure contenant différents paramètres pour usage des variables de  conditions
typedef struct {
	int val;	//valeur de la variable de condition
	pthread_mutex_t mutex;	//mutex de la variable de condition
	pthread_cond_t cond;	//variable de condition
} cond_t;

//Permet de savoir quand le tri est terminé et d'agir en conséquence
cond_t tri_termine;

//Mutex permettant de protéger la variable nbr_attente
pthread_mutex_t mutexNbrAttente = PTHREAD_MUTEX_INITIALIZER;

//Structure contenant les différents paramètres passés à chaque thread.
typedef struct {
	int num_thread;	//Numero du thread
	cond_t *cond;	//Tableau contenant les variable de condition pour protéger les zones adjacentes
	pthread_mutex_t *mutex;	//Tableau qui contient les mutex
	int debut;	//Indice de debut de la zone
	int longueur;	//Indice de longueur de la zone
} parametres;

clock_t begin;	//Temps du début de tri du tableau
clock_t end;	//Temps de fin de tri du tableau
double temps_total;	//Mesure du temps écoulé


/**
* Protypes fonctions
*/
void cond_init(cond_t *s);
void cond_post(cond_t *s);
void cond_wait(cond_t *s);

void *t_zone(void *arg);	//thread qui s'occupe du trie d'une zone

bool validerTri();
void tri_a_bulle (parametres *para);

int randomNbr(int max);
void remplirTableau(int *tableau, int tab_size);
void affichageTableau(int *tableau, int tab_size);


/**
* Point d'entrée du programme qui initialise tous les sous-threads triant le tableau
* @param argc nombre d'arguments
* @param argv tableau de string qui stock les arguments
* @return code d'execution du programme
*/
int main(int argc, char *argv[])
{

	if (argc != 3) {
    fprintf(stderr, "usage: %s [tab_size] [nb_threads]\n", argv[0]);
    return EXIT_FAILURE;
  }

	tab_size = atoi(argv[1]);	//taille du tableau
	nbr_zones = atoi(argv[2]); //nbr de zones pour le trier (threads)

	if ((nbr_zones > tab_size-1 || nbr_zones < 1)) {
		fprintf(stderr, "le nombre de threads doit être compris entre 1 et %d.\n", tab_size-1);
    return EXIT_FAILURE;
	}

	//Création du thread principal
	int *zones;
	parametres *p = (parametres *)malloc(sizeof(parametres));
	parametres *tabParam = (parametres *)malloc(sizeof(parametres));
	pthread_t *subthread;
	int error;

	printf("tri à bulles à l'aide de plusieurs threads\n\n");

	//Initilisation de la mémoire pour les différents tableaux
	tableau = malloc(tab_size * sizeof(int));
	subthread = malloc(nbr_zones * sizeof(pthread_t));
	zones = malloc(nbr_zones * sizeof(int));
	tabParam = malloc(nbr_zones * sizeof(parametres));
	p->mutex = malloc(nbr_zones * sizeof(pthread_mutex_t));
	p->cond = malloc(nbr_zones * sizeof(cond_t));
	cond_init(&tri_termine);

	remplirTableau(tableau, tab_size);	//rempli le tableau de valeur aléatoires

	printf("Le tableau non trié : \n");
	affichageTableau(tableau, tab_size);	//affiche tableau

	//Initialisation du tableau contenant la taille des zones et des mutex des zones communes.
	int tailleZone = (tab_size+nbr_zones-1) / nbr_zones;	//taille des zones
	int reste = (tab_size+nbr_zones-1) % nbr_zones;	//reste à dispatcher entre les différentes zones

	//Début du chrono
	begin = clock();

	int i;
	for (i=nbr_zones-1; i>=0; i--)
	{
		if (reste != 0)
		{
			zones[i] = tailleZone + 1;
			reste--;
		}
		else {
			zones[i] = tailleZone;
		}
		cond_init(&p->cond[i]);
		if (i!=nbr_zones-1)
			pthread_mutex_init(&p->mutex[i], NULL);
	}

	//Initialisation des différents threads
	int debut = 0;

	for (i=0; i<nbr_zones;i++)
	{
		p->num_thread = i;
		p->longueur = zones[i];
		if (i==0)
			//Création de la première zone
			p->debut = 0;
		else
		{
			//Création autres zones
			debut += zones[i-1]-1;
			p->debut = debut;
		}
		tabParam[i] = *p;
		//Test si il y une erreur lors de la création
		if ((error = pthread_create(&subthread[i], NULL, t_zone,
				(void*)&tabParam[i])) != 0)
			printf("Erreur lors de la creation du thread no %d : %d\n", i, error);
		else
			printf("Le thread de la zone N°%d initialise.\n", i);
	}

	cond_wait(&tri_termine);	//attend que nb threads attente = nbthread

	//Fin du chronométrage
	end = clock();
	temps_total = ((double) (end - begin)) / CLOCKS_PER_SEC;

	for (i=0; i<nbr_zones;i++)
	{
		cond_post(&(p->cond[i]));
	}

	//Attente de la fin de tous les threads
	for (i=0; i<nbr_zones;i++)
	{
		pthread_join(subthread[i], NULL);
	}

	printf("\nLe tableau trié: \n");
	affichageTableau(tableau, tab_size);

	printf("Temps de tri du tableau : %f\n", temps_total);

	//Validation du tri
	if(validerTri())
		printf("Le tri s'est effectue correctement!\n\n");
	else
		printf("Le tri ne s'est pas effectue correctement...\n\n");

	return 1;
}


//permet de mettre un thread en attente passive
void cond_wait(cond_t *s) {
	pthread_mutex_lock(&s->mutex);
	// Si valeur est à 0 on se met en attente
	while (s->val == 0) {
		pthread_cond_wait(&s->cond, &s->mutex);
	}
	s->val--;
	pthread_mutex_unlock(&s->mutex);
}

//permet de libérer un thread
void cond_post(cond_t *s) {
	pthread_mutex_lock(&s->mutex);
	s->val++;
	pthread_cond_signal(&s->cond);
	pthread_mutex_unlock(&s->mutex);
}

//permet d'initialiser un thread
void cond_init(cond_t *s) {
	s->val = 0; //0 par defaut
	pthread_mutex_init(&s->mutex, NULL);
	pthread_cond_init(&s->cond, NULL);
}


/**
* thread pour trier une zone du tableau
* @param arg paramètres nécessaires au thread
* @return nombre aléatoire calculé
*/
void *t_zone(void *arg)
{
	while (nbr_attente != nbr_zones)
	{
		parametres* p = arg;
		tri_a_bulle(p);
		pthread_mutex_lock(&mutexNbrAttente);
		nbr_attente++;
		pthread_mutex_unlock(&mutexNbrAttente);

		//Si tous les threads sont en attente, on signal au thread principal d'arreter le tri
		if (nbr_attente == nbr_zones) {
			cond_post(&tri_termine);
		} else {
			cond_wait(&(p->cond[p->num_thread]));	//Attente jusqu a ce qu'une zone commune de la zone soit modifiée
		}
	}
}

/**
* Effectue le tri à bulle sur une zone
* @param arg paramètres nécessaires pour trier la bonne zone
* @return ne retourne rien
*/
void tri_a_bulle(parametres *para)
{
	int num_thread = para->num_thread;	//numéro du thread
	int * tab = &tableau[para->debut];	//case de départ du tableau pour cette zone tri
	int longueurZone = para->longueur;	//nombre de cases de la zone de tri
	int i, j;	//indices boucles
	int valeurPartageHaut, zonePartageBas;	//zone partagée du haut et du bas

	for (i=longueurZone-1; i>0; i--)
	{
		//Test si on modifie la zone commune du "haut"
		if (i== longueurZone-1 && num_thread+1!=nbr_zones)
		{
			pthread_mutex_lock(&para->mutex[num_thread]);
			valeurPartageHaut = tab[i];
		}

		for (j=0; j<i; j++)
		{
			//Test si on modifie la zone commune du "bas"
			if (num_thread !=0 && j==0)
			{
				pthread_mutex_lock(&para->mutex[num_thread-1]);
				zonePartageBas = tab[j];
			}
			//Tri
			if (tab[j]>tab[i])
			{
				int temp = tab[j];
				tab[j] = tab[i];
				tab[i] = temp;
			}
			//Test si la zone commune du "bas" a été modifiée
			if (num_thread != 0 && j == 0)
			{
				if (tab[j] != zonePartageBas)
				{
					cond_post(&para->cond[num_thread-1]);
					pthread_mutex_lock(&mutexNbrAttente);
					nbr_attente--;
					pthread_mutex_unlock(&mutexNbrAttente);
				}
				pthread_mutex_unlock(&para->mutex[num_thread-1]);
			}
		}
		//Test si la zone commune du "haut" a été modifiée
		if (i== longueurZone-1 && num_thread+1!=nbr_zones)
		{
			if (tab[i] != valeurPartageHaut)
			{
				cond_post(&para->cond[num_thread+1]);
				pthread_mutex_lock(&mutexNbrAttente);
				nbr_attente--;
				pthread_mutex_unlock(&mutexNbrAttente);
			}
			pthread_mutex_unlock(&para->mutex[num_thread]);
		}
	}
}

/**
* Retourner un entier aléatoire
* @param max nombre maximum possible
* @return nombre aléatoire
*/
int randomNbr(int max)
{
	return (rand() % (max + 1));
}

/**
* Valider si le tri du tableau est OK
* @return true si le tableau est trié, sinon false
*/
bool validerTri()
{
	int i;
	for (i=0; i<tab_size-1; i++)
	{
		if (tableau[i] > tableau[i+1])	//test si la case supérieur est plus petite
			return false;
	}
	return true;
}

/**
* affiche toutes les valeurs du tableau
* @return ne retourne rien
*/
void affichageTableau(int *tableau, int tab_size)
{
	//Affichage du tableau
	int i;
	for (i=0; i<tab_size; i++)
	{
		printf("|");
		printf("%d", tableau[i]);
	}
	printf("|\n\n");
}


/**
* rempli le tableau de valeurs aléatoires
* @return ne retourne rien
*/
void remplirTableau(int *tableau, int tab_size)
{
	int i;
	for (i=0; i<tab_size; i++)
	{
		tableau[i] = randomNbr(nbr_maximum);
	}
}
