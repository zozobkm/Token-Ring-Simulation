#ifndef ANNEAU_H
#define ANNEAU_H

// 1. L'énumération des types de messages
// Le prof demandera : "Pourquoi utiliser un enum ?"
// Réponse : "Pour la lisibilité. Au lieu de dire 'type 1' ou 'type 2', on utilise des mots clairs. C'est plus robuste."
typedef enum {
    MSG_TOKEN,      // Le fameux jeton (celui qui l'a, parle)
    MSG_DATA,       // Un message texte d'une machine à une autre (Unicast)
    MSG_DIFFUSION,  // Un message texte pour tout le monde (Broadcast)
    MSG_FICHIER,    // Un morceau de fichier binaire
    MSG_INFO,       // Demande de l'état du réseau (Option 5)
    MSG_INSERTION,  // Message système : "Hé, une nouvelle machine arrive !"
    MSG_RETRAIT     // Message système : "Hé, je quitte le réseau !"
} TypeMsg;

// 2. La structure pour découper les fichiers
// Le prof demandera : "Pourquoi on ne transfère pas le fichier d'un coup ?"
// Réponse : "Pour l'équité ! Si on envoie 1 Go d'un coup, on monopolise le jeton. On découpe donc en blocs de 512 octets."
struct p_fichier {
    char nom[256];
    int num_bloc;         // Pour remettre les morceaux dans l'ordre
    int dernier_bloc;     // Vaut 1 si c'est la fin du fichier, sinon 0
    char data[512];       // La donnée brute
};

// 3. Le Paquet réseau (Ce qui voyage dans les câbles)
struct paquet {
    TypeMsg type;
    int src_id;           // Qui a envoyé ?
    int dest_id;          // Pour qui c'est ? (-1 pour tout le monde)
    int taille_payload;   // La taille utile (surtout pour le dernier bloc de fichier)
    
    // Le prof demandera : "Pourquoi une 'union' et pas un 'struct' ici ?"
    // Réponse : "Pour optimiser la RAM et le réseau ! Un message ne peut pas être 
    // à la fois un texte ET un fichier. L'union prend la taille du plus gros (1024 octets) 
    // et on utilise soit l'un, soit l'autre. Ça évite d'envoyer des octets vides."
    union {
        char texte[1024];
        struct p_fichier fichier;
    } payload;
};

#endif