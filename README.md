# Token-Ring-Simulation : Simulation d'Architecture Réseau en Anneau (C)

## Description
Token-Ring-IPC-Network est une implémentation robuste et dynamique du protocole réseau en anneau (inspiré du Token Ring IEEE 802.5), développée intégralement en C bas niveau.

Ce projet simule des machines interconnectées de façon dynamique (connexion et déconnexion à chaud) en utilisant l'API des Sockets TCP. Il démontre une maîtrise avancée de la programmation système Linux, de la Communication Inter-Processus (IPC), du multiplexage d'entrées/sorties, et de la gestion de la mémoire.

---

## Fonctionnalités Clés
- Topologie Dynamique : Les nœuds peuvent rejoindre ou quitter l'anneau réseau à tout moment grâce à une auto-reconfiguration des voisins de droite et de gauche via des messages de contrôle d'insertion et de retrait.
- Mechanism de Watchdog (Régénération de Jeton) : Implémentation d'un mécanisme de tolérance aux pannes. Si le jeton est perdu (timeout de 5 secondes détecté via la fonction select), le nœud moniteur principal (Active Monitor) régénère automatiquement un nouveau jeton pour relancer l'anneau.
- Multiplexage I/O Asynchrone : Utilisation de la fonction select pour écouter simultanément les requêtes clavier (stdin), les connexions entrantes IPC, et le trafic de l'anneau sans jamais bloquer l'exécution des processus.
- Transfert de Fichiers Binaire (Chunking) : Découpage des fichiers lors du transfert en blocs de 512 octets pour garantir l'équité du réseau (ne pas monopoliser le jeton de parole) et reconstruction binaire stricte à la réception.
- Modes de Communication : Support complet de l'Unicast (message direct à un nœud spécifique) et du Broadcast (diffusion de messages texte à tout l'anneau).

---

## Architecture Logicielle
L'architecture de chaque nœud est scindée en deux processus distincts qui communiquent en local via des sockets IPC pour garantir la modularité et la résilience :

1. Le Driver (driver.c) : Le démon réseau (Daemon) fonctionnant en arrière-plan. Il gère l'état et la topologie de l'anneau (Sockets TCP entrantes et sortantes), le passage du jeton, et le routage des différents paquets de données.
2. Le Comm (comm.c) : L'interface utilisateur (Client). Il se connecte au Driver via une socket locale (IPC sur le port 9000 + ID), capture les entrées de l'utilisateur au clavier, et affiche les informations et messages reçus depuis l'anneau.

---

## Optimisations Système
- Empreinte mémoire minimisée : La structure de données réseau (struct paquet) utilise une union C pour le payload. Un paquet ne pouvant être simultanément un message texte et un bloc de fichier, la taille maximale du paquet est figée à 1024 octets stricts, optimisant l'usage de la bande passante.
- Socket Reuse (SO_REUSEADDR) : Configuration des options de sockets pour permettre le redémarrage et la reconnexion immédiate des processus sans subir le timeout de libération des ports par le noyau Linux.
- Traitement des signaux système : Masquage volontaire du signal SIGPIPE (via SIG_IGN) pour empêcher le crash ou l'extinction brutale du processus Driver si le processus Comm associé vient à se fermer de manière inattendue.

---

## Compilation et Utilisation

### 1. Compilation
Utilisez le compilateur GCC pour générer séparément les deux exécutables du projet :
```bash
gcc -o driver driver.c
gcc -o comm comm.c


2. Lancement d'un Nœud (Exemple pour le Nœud 1)
Pour exécuter un nœud sur votre machine, ouvrez deux terminaux distincts.

Terminal A (Lancement du Démon Réseau) :
Format de la commande : ./driver  <Mon_IP> <Mon_Port> <IP_Voisin_Droite> <Port_Voisin_Droite>

Bash
./driver 1 127.0.0.1 5001 127.0.0.1 5002
Terminal B (Lancement de l'Interface Utilisateur) :

Bash
./comm 1
3. Création de l'anneau
Pour étendre le réseau, lancez d'autres terminaux pour instancier les nœuds suivants (ID 2, ID 3, etc.) en veillant à boucler la topologie (le port cible du tout dernier nœud doit pointer vers le port d'écoute initial du Nœud 1).

Licence
Projet académique réalisé dans le cadre de l'UE Protocoles Réseaux. Libre d'accès pour l'étude, la modification et le déploiement sur des maquettes d'infrastructures virtualisées.
