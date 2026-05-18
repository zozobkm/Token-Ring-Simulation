#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "anneau.h"

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: ./comm <ID>\n"); exit(1); }
    int id = atoi(argv[1]);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // Le prof demandera : "Comment Comm et Driver communiquent entre eux ?"
    // Réponse : "C'est de l'IPC (Communication Inter-Processus) via des Sockets TCP locales.
    // On a fixé le port à 9000 + ID (Ex: Le Comm 1 parle sur le port 9001)."
    struct sockaddr_in addr = {AF_INET, htons(9000 + id), inet_addr("127.0.0.1")};
    
    while(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) sleep(1);
    printf("Comm %d Connecte au Driver\n", id);

    fd_set readfs;
    while(1) {
        printf("\n1. Emettre | 2. Diffuser | 3. Fichier | 4. Quitter | 5. Info\nChoix: ");
        fflush(stdout);
        
        FD_ZERO(&readfs); 
        FD_SET(0, &readfs); // On écoute "0" (C'est l'entrée clavier standard !)
        FD_SET(sock, &readfs); // On écoute la socket du Driver
        
        // Encore select() pour ne pas bloquer le clavier si on reçoit un message
        select(sock + 1, &readfs, NULL, NULL, NULL);

        // --- 1. LE DRIVER NOUS DONNE UN MESSAGE ---
        if (FD_ISSET(sock, &readfs)) {
            struct paquet r;
            if (recv(sock, &r, sizeof(struct paquet), 0) > 0) {
                if (r.type == MSG_DATA || r.type == MSG_DIFFUSION) 
                    printf("\n[RECU de %d] : %s\n", r.src_id, r.payload.texte);
                else if (r.type == MSG_INFO) 
                    printf("\n--- ETAT ANNEAU ---\n%s------------------\n", r.payload.texte);
                else if (r.type == MSG_FICHIER) {
                    // Reconstruction du fichier
                    // "wb" (Write Binary) si c'est le bloc 0, "ab" (Append) pour la suite
                    FILE *f = fopen(r.payload.fichier.nom, (r.payload.fichier.num_bloc == 0) ? "wb" : "ab");
                    if (f) { 
                        fwrite(r.payload.fichier.data, 1, r.taille_payload, f); 
                        fclose(f); 
                    }
                    if (r.payload.fichier.dernier_bloc) 
                        printf("[FICHIER] '%s' recu.\n", r.payload.fichier.nom);
                }
            } else break; // Si on sort du recv() avec <=0, le Driver a planté
        }

        // --- 2. L'UTILISATEUR TAPE AU CLAVIER ---
        if (FD_ISSET(0, &readfs)) {
            int choix; scanf("%d", &choix);
            struct paquet p; memset(&p, 0, sizeof(struct paquet)); p.src_id = id;
            
            if (choix == 4) { p.type = MSG_RETRAIT; send(sock, &p, sizeof(struct paquet), 0); break; }
            if (choix == 5) { p.type = MSG_INFO; send(sock, &p, sizeof(struct paquet), 0); }
            
            else if (choix == 1 || choix == 2) {
                p.type = (choix == 1) ? MSG_DATA : MSG_DIFFUSION;
                if (choix == 1) { printf("Dest: "); scanf("%d", &p.dest_id); }
                else p.dest_id = -1; // -1 c'est la convention pour la diffusion
                
                printf("Message: "); getchar(); // Pour consommer le 'Entrée' du scanf
                fgets(p.payload.texte, 1024, stdin);
                p.payload.texte[strcspn(p.payload.texte, "\n")] = 0; // Enlève le saut de ligne
                send(sock, &p, sizeof(struct paquet), 0);
            } 
            else if (choix == 3) {
                char path[256]; printf("Dest: "); scanf("%d", &p.dest_id);
                printf("Fichier: "); scanf("%s", path);
                FILE *f = fopen(path, "rb"); // Read Binary
                if (!f) continue;
                
                p.type = MSG_FICHIER; 
                snprintf(p.payload.fichier.nom, 256, "copie_%.240s", path); // On renomme à l'arrivée
                
                // Le prof demandera : "Comment vous gérez la fin d'un fichier ?"
                // Réponse : "fread nous donne le nombre d'octets lus. On vérifie 
                // avec fgetc s'il y a encore un truc après. Sinon, on met dernier_bloc à 1."
                while ((p.taille_payload = fread(p.payload.fichier.data, 1, 512, f)) > 0) {
                    int c = fgetc(f); 
                    p.payload.fichier.dernier_bloc = (c == EOF);
                    if (c != EOF) ungetc(c, f); // Si c'est pas fini, on remet la lettre lue
                    
                    send(sock, &p, sizeof(struct paquet), 0); 
                    p.payload.fichier.num_bloc++;
                    usleep(10000); // TRES IMPORTANT : Laisse le Driver et le réseau respirer
                }
                fclose(f);
            }
        }
    }
    close(sock); return 0;
}