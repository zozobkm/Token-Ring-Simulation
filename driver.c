#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "anneau.h"

int main(int argc, char *argv[]) {
    // --- 1. LECTURE DES ARGUMENTS ---
    // Le prof demandera : "Pourquoi passer l'IP en argument ?"
    // Réponse : "Pour que les machines puissent s'envoyer leur IP lors de l'insertion. 
    // Sans ça, une machine distante ne saurait pas comment se connecter au nouveau."
    if (argc < 6) { 
        printf("Usage: %s <ID> <Mon_IP> <Mon_Port> <IP_Cible> <Port_Cible>\n", argv[0]); 
        exit(1); 
    }
    
    // Ignore le signal SIGPIPE pour éviter que le programme plante si le processus Comm se ferme brutalement.
    signal(SIGPIPE, SIG_IGN); 

    int mon_id = atoi(argv[1]);
    char mon_ip[64]; strncpy(mon_ip, argv[2], 63);   // Mon adresse IP (ex: 192.168.1.10)
    int p_g = atoi(argv[3]);                         // Mon port d'écoute (Port Gauche)
    
    char ip_d[64];  strncpy(ip_d, argv[4], 63);      // L'IP de mon voisin de droite
    int p_d = atoi(argv[5]);                         // Le port de mon voisin de droite

    // --- 2. CREATION DES SOCKETS SERVEUR (Pour écouter) ---
    // sg = Socket Gauche : C'est ici qu'on attend que le voisin précédent se connecte.
    int sg = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM = TCP (pour un transfert sans erreur)
    struct sockaddr_in addr_g = {AF_INET, htons(p_g), INADDR_ANY};
    int opt = 1;
    
    // Le prof demandera : "A quoi sert SO_REUSEADDR ?"
    // Réponse : "Ça permet de relancer le programme immédiatement sans avoir l'erreur 
    // 'Address already in use' si le port n'a pas été libéré assez vite par l'OS."
    setsockopt(sg, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(sg, (struct sockaddr *)&addr_g, sizeof(addr_g));
    listen(sg, 5);

    // sl = Socket Locale : C'est ici qu'on attend notre processus `comm` (l'interface utilisateur).
    int sl = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr_l = {AF_INET, htons(9000 + mon_id), inet_addr("127.0.0.1")}; // Toujours en local
    setsockopt(sl, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(sl, (struct sockaddr *)&addr_l, sizeof(addr_l));
    listen(sl, 1);

    // --- 3. CONNEXION AU VOISIN DE DROITE (Client) ---
    // sock_d = Socket Droite : On essaie de se brancher sur la machine suivante
    int sock_d = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr_d = {AF_INET, htons(p_d), inet_addr(ip_d)};
    
    printf("Driver %d: Mon IP est %s:%d. Tentative de connexion vers %s:%d...\n", mon_id, mon_ip, p_g, ip_d, p_d);
    // On boucle tant que l'autre machine n'est pas allumée
    while(connect(sock_d, (struct sockaddr *)&addr_d, sizeof(addr_d)) == -1) sleep(1);

    // --- 4. MESSAGE D'INSERTION ---
    // Je viens de me connecter. J'envoie un message sur l'anneau pour dire :
    // "Attention, celui qui pointait vers ma cible doit maintenant pointer vers MOI".
    struct paquet p_ins;
    memset(&p_ins, 0, sizeof(struct paquet));
    p_ins.type = MSG_INSERTION;
    p_ins.src_id = mon_id; 
    p_ins.dest_id = p_d;           // L'ancien port cible
    p_ins.taille_payload = p_g;    // Mon port (le nouveau port cible)
    strncpy(p_ins.payload.texte, mon_ip, 63); // Mon IP (la nouvelle IP cible)
    send(sock_d, &p_ins, sizeof(struct paquet), 0);

    // On accepte enfin les connexions entrantes (Anneau + Interface)
    int sock_g = accept(sg, NULL, NULL); 
    int sock_comm = accept(sl, NULL, NULL); 
    printf("Driver %d: *** ANNEAU PRET ***\n", mon_id);

    // --- 5. INITIALISATION DU JETON ---
    // Le prof demandera : "Qui crée le jeton au départ ?"
    // Réponse : "La machine avec l'ID 1. C'est notre contrôleur principal (Active Monitor)."
    if (mon_id == 1) {
        struct paquet p_init; memset(&p_init, 0, sizeof(struct paquet));
        p_init.type = MSG_TOKEN;
        send(sock_d, &p_init, sizeof(struct paquet), 0);
    }

    fd_set readfs; // L'ensemble des "fichiers/sockets" qu'on veut surveiller
    struct paquet msg_pret;
    int a_envoyer = 0, veut_quitter = 0;

    // --- 6. LA BOUCLE PRINCIPALE (MULTIPLEXAGE) ---
    while(1) {
        FD_ZERO(&readfs);
        FD_SET(sg, &readfs); // On surveille le port d'écoute principal (pour les nouveaux)
        if (sock_g != -1) FD_SET(sock_g, &readfs); // On surveille le voisin de gauche
        if (sock_comm != -1) FD_SET(sock_comm, &readfs); // On surveille l'interface Comm
        else FD_SET(sl, &readfs); // Ou on attend que Comm se reconnecte

        // Le prof demandera : "A quoi sert le select() exactement ?"
        // Réponse : "Il permet d'attendre des données sur plusieurs sockets en même temps 
        // sans bloquer le programme. Si on utilisait juste recv(), le programme serait 
        // bloqué en attendant un message et ne pourrait rien faire d'autre."
        struct timeval tv = {5, 0}; // 5 secondes et 0 microsecondes
        int res = select(FD_SETSIZE, &readfs, NULL, NULL, &tv);

        // --- 7. GESTION DU WATCHDOG (Perte du Jeton) ---
        // Le prof demandera : "Comment le système réagit si on perd le jeton ?"
        // Réponse : "select() renvoie 0 s'il y a un timeout (5s sans rien recevoir). 
        // Si ça arrive, la Machine 1 régénère un nouveau jeton pour relancer l'anneau."
        if (res == 0) {
            if (mon_id == 1 && sock_d != -1) {
                struct paquet p_new; memset(&p_new, 0, sizeof(struct paquet));
                p_new.type = MSG_TOKEN;
                send(sock_d, &p_new, sizeof(struct paquet), 0);
            }
            continue; 
        }

        // Si on reçoit une NOUVELLE connexion sur le port Gauche (Insertion)
        if (FD_ISSET(sg, &readfs)) {
            int n_sock = accept(sg, NULL, NULL);
            if (sock_g != -1) close(sock_g);
            sock_g = n_sock; // Le nouveau venu devient notre voisin de gauche
            continue; 
        }

        // Si l'interface Comm veut se reconnecter
        if (sock_comm == -1 && FD_ISSET(sl, &readfs)) {
            sock_comm = accept(sl, NULL, NULL);
            continue;
        }

        // --- 8. MESSAGES VENANT DE L'INTERFACE UTILISATEUR (COMM) ---
        if (sock_comm != -1 && FD_ISSET(sock_comm, &readfs)) {
            if (recv(sock_comm, &msg_pret, sizeof(struct paquet), 0) <= 0) {
                close(sock_comm); sock_comm = -1; // Le Comm a été fermé
            } else {
                if (msg_pret.type == MSG_RETRAIT) {
                    // Si on veut quitter, on prévient l'anneau et on donne l'IP/Port de notre cible
                    // pour que notre voisin de gauche s'y connecte
                    struct paquet p_quit; memset(&p_quit, 0, sizeof(struct paquet));
                    p_quit.type = MSG_RETRAIT; p_quit.src_id = mon_id; 
                    p_quit.dest_id = p_g; p_quit.taille_payload = p_d;
                    strncpy(p_quit.payload.texte, ip_d, 63); 
                    send(sock_d, &p_quit, sizeof(struct paquet), 0);
                    veut_quitter = 1; // On lèvera le drapeau pour s'éteindre quand le message fera le tour
                } 
                else if (msg_pret.type == MSG_INFO) {
                    // On prépare nos infos pour la commande 5 (Etat du réseau)
                    snprintf(msg_pret.payload.texte, 1024, "Machine: %d | IP: %s | Port E: %d | Port S: %d\n", mon_id, mon_ip, p_g, p_d);
                    a_envoyer = 1;
                }
                else a_envoyer = 1; // Data ou Fichier
            }
        }

        // --- 9. MESSAGES VENANT DE L'ANNEAU (VOISIN GAUCHE) ---
        if (sock_g != -1 && FD_ISSET(sock_g, &readfs)) {
            struct paquet p;
            if (recv(sock_g, &p, sizeof(struct paquet), 0) <= 0) {
                // Si le voisin se déconnecte brutalement ou si on a demandé à quitter
                if (veut_quitter) exit(0); 
                close(sock_g); sock_g = -1; continue; 
            }

            // GESTION INSERTION : On doit changer de voisin de droite
            if (p.type == MSG_INSERTION) {
                if (p.src_id == mon_id) continue; // Le message a fait un tour, on l'arrête
                // Si le message est pour moi (ma cible a été remplacée)
                if (p.dest_id == p_d) {
                    close(sock_d); 
                    p_d = p.taille_payload; // Le nouveau port
                    strncpy(ip_d, p.payload.texte, 63); // La nouvelle IP
                    
                    sock_d = socket(AF_INET, SOCK_STREAM, 0);
                    struct sockaddr_in addr_new = {AF_INET, htons(p_d), inet_addr(ip_d)};
                    // Je me reconnecte dynamiquement au nouveau voisin
                    while(connect(sock_d, (struct sockaddr *)&addr_new, sizeof(addr_new)) == -1) usleep(100000);
                } else send(sock_d, &p, sizeof(struct paquet), 0); // Sinon je fais circuler
            }
            
            // GESTION RETRAIT : Mon voisin s'en va, je le saute
            else if (p.type == MSG_RETRAIT) {
                if (p.src_id == mon_id) exit(0); // C'est moi qui l'ai envoyé, je peux m'éteindre
                if (p.dest_id == p_d) {
                    close(sock_d); 
                    p_d = p.taille_payload; // Le port de MA cible de MA cible
                    strncpy(ip_d, p.payload.texte, 63); // L'IP de MA cible de MA cible
                    
                    sock_d = socket(AF_INET, SOCK_STREAM, 0);
                    struct sockaddr_in addr_new = {AF_INET, htons(p_d), inet_addr(ip_d)};
                    while(connect(sock_d, (struct sockaddr *)&addr_new, sizeof(addr_new)) == -1) usleep(100000);
                } else send(sock_d, &p, sizeof(struct paquet), 0);
            }
            
            // GESTION DU JETON
            // Règle Token Ring : Je ne peux envoyer un truc à moi QUE quand je reçois ce message
            else if (p.type == MSG_TOKEN) {
                if (a_envoyer) { 
                    send(sock_d, &msg_pret, sizeof(struct paquet), 0); 
                    a_envoyer = 0; 
                }
                send(sock_d, &p, sizeof(struct paquet), 0); // On fait passer le jeton
            } 
            
            // GESTION DE LA RECOLTE D'INFOS (Option 5)
            else if (p.type == MSG_INFO) {
                if (p.src_id == mon_id) { // C'est moi qui l'ai demandé, j'affiche le résultat
                    if (sock_comm != -1) send(sock_comm, &p, sizeof(struct paquet), 0);
                } else {
                    // Je rajoute mes propres informations à la fin du texte existant
                    char b[256]; snprintf(b, sizeof(b), "Machine: %d | IP: %s | Port E: %d | Port S: %d\n", mon_id, mon_ip, p_g, p_d);
                    if (strlen(p.payload.texte) + strlen(b) < 1024) strcat(p.payload.texte, b);
                    send(sock_d, &p, sizeof(struct paquet), 0);
                }
            }
            
            // GESTION CLASSIQUE (Messages et fichiers)
            else {
                // Si c'est pour moi (ou broadcast), je donne à mon Interface Utilisateur
                if (p.dest_id == mon_id || p.type == MSG_DIFFUSION) {
                    if (sock_comm != -1) send(sock_comm, &p, sizeof(struct paquet), 0);
                }
                // Si ça vient d'un autre, je fais suivre sur l'anneau
                if (p.src_id != mon_id) send(sock_d, &p, sizeof(struct paquet), 0);
            }
        }
    }
    return 0;
}