# Project m1: Remote backup (BACKUP)

## Project’s summary
The project aims at building a client-server system that performs an incremental back-up
of the content of a folder (and all its sub-folders) on the local computer onto a remote
server. Once launched, the system will operate as a background service keeping the content
of the configured folder in sync with the remote copy: whenever the monitored content
changes (new files are added, existing ones are changed and/or removed), suitable
command will be sent across the network in order to replicate the changes on the other
side. In case of transient errors (network portioning or failure of the remote server), the
system will keep on monitoring the evolution of the local folder and try to sync again as
soon as the transient error condition vanishes).
## Required Background and Working Environment
Knowledge of the C++17 general abstractions and of the C++ Standard Template Library.
Knowledge of concurrency, synchronization and background processing.
The system will be developed using third party libraries (e.g., boost.asio) in order to
support deployment on several platforms.
## Problem Definition
### Overall architecture
The system consists of two different modules (the client and the server) that interact via a
TCP socket connection.
A dialogue takes place along this connection allowing the two parties to understand eachother.
The set of possible messages, their encoding and their meaning represents the so
called “application-level protocol” which should be defined first, when approaching this
kind of problems.
Its content stems from the overall requirements of the system and should allow a suitable
representation for the envisaged entities which can be part of the conversation (amongst
the others, files – comprising their name, their content and their metadata, folders,
commands, responses, error indications, …). The set of possible requests and the
corresponding responses should be defined (and probably, refined along the project, as
more details emerge).
Moreover, each exchanged message should also be related with the implications it has on
the receiving party, that should be coded taking such an implication as a mandatory
requirement.
In order to remain synced, both the client and the server need to have an image of the
monitored files as they are stored in the respective file system. The application protocol
should offer some kind of “probe command” that allows the client to verify whether or not
the server already has a copy of a given file or folder. Since the filename and size are not
enough to proof that the server has the same copy as the client, a (robust) checksum or
hash should be computed per each file and sent to the server. Since the server may need
some time to compute the answer, the protocol should probably operate in asynchronous
mode, i.e., without requiring an immediate response of the server before issuing a new
request to it.

## Client Side
The client side is in charge of continuously monitoring a specific folder that can be specified
in any reasonable way (command line parameter, configuration file, environment
variable…) and check that all the contents are in sync with the sever side. To perform this
operation, it can rely on the filesystem class provided with the C++17 library or the Boost
one (https://www.boost.org/doc/libs/1_73_0/libs/filesystem/doc/index.htm). Whenever
a discrepancy is found, the local corresponding entry should be marked as invalid and
some arrangements should be taken to transfer the (updated) file to the server. Some
indications on how to create a file system watcher can be found here
(https://solarianprogrammer.com/2019/01/13/cpp-17-filesystem-write-file-watcher-monitor/)
## Server Side
The server side is responsible of listening on socket connection and accept connection
requests from clients. The server should be designed in order to manage more than one
client and have separate backup folders for each of them. When the communication
channel is setup, a check of the client identity will be performed (by exchanging a name and
some form of proof of identity) in order to associate the connection to the proper
configuration parameters. Per each existing connection, incoming messages will be
evaluated in the order they arrive and a suitable response will be generated, either
containing the requested information or an error code. In case of success, any implication
of the command will be guaranteed. In case of error, no change will happen on the server
side.
Communication between the client and the server can be based on the Boost ASIO library
(https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio.html) or any other
suitable one.

# Appunti

### da capire
* cosa si intende per background service (se è un semplice programma con cui non si interagisce o di più.. nel primo
caso bisogna anche capire se si deve rendere invisibile la finestra di comando del programma o no)
* se un utente può accedere allo stesso servizio da più macchine differenti.. in tal caso sarebbe da gestire
il fatto che quando un utente si connette al servizio per la prima volta su una macchina il contenuto della
cartella da monitorare potrebbe essere diverso (o anche vuoto).. che fare in questo caso? Una possibile soluzione
potrebbe essere lanciare il programma con un comando specifico che per prima cosa crea la cartella (se non esiste
già) e scarica tutto ciò che c'è sul server sul client in modo che siano sincronizzati e abbiano lo stesso contenuto;
successivamente il programma si comporta normalmente.
* se è possibile per un utente essere connesso al servizio su più macchine differenti contemporaneamente e in tal caso 
se è da gestire la modifica in una cartella remota (su server) di file.. in tal caso tale modifica sarebbe da propagare
alle macchine (client) connesse per lo stesso utente (tutto è più semplice se un utente può essere connesso
al servizio da un unica macchina alla volta)

### modifiche da monitorare sul file system
tipo di modifica | azione da eseguire (su client)
------------ | -------------
creazione di un file | calcolo hash del file (considerando file name, data di ultima modifica, file size) + verifica che il server non abbia già una copia di tale file + invio comando di creazione file (file + hash) 
modifica di un file | calcolo nuova hash del file (file name, date, size) + verifica che il server non abbia già una copia di tale file + invio comando di modifica file (file + hash)
rinominazione di un file | calcolo nuova hash del file (file name, date, size) + verifica che il server non abbia già una copia di tale file + invio comando di ridenominazione file (+ new file name  hash)
eliminazione di un file | verifica che il server abbia una copia di tale file + invio comando di eliminazione file
spostamento di un file in un altra cartella (monitorata) | (hash non cambia, già calcolato, se serve si può ricalcolare) invio comando spostamento file (src + dst)
creazione di una cartella | invio comando creazione cartella
eliminazione di una cartella | invio comando eliminazione cartella (+ recursive o no)
spostamento di una cartella | invio comando spostamento cartella (src + dst)
rinominazione di una cartella | invio comando ridenominazione cartella

### messaggi
* #### struttura
1 byte | 1byte | 1 byte | x bytes |
--- | --- | --- | ---
total len | version | type | content

* #### tipi
codice | significato | content | effetti | src | dst
--- | --- | --- | --- | --- | ---
FC | file create | file metadata (name, path relative to backup root, date, size etc.)+ file + file hash | crea file sul server | C | S
FE | file edit | file metadata (name, path relative to backup root, date, size etc.) + file + previous file hash + new file hash | sovrascrivi (o cancella e poi scrivi) file esistente sul server (modificandolo) | C | S
FR | file rename | file metadata (name, path relative to backup root, date, size etc.) + prev file hash + new file hash | rinomina file esistente sul server | C | S
FD | file delete | file hash | elimina file da server | C | S
FM | file move | (new) file metadata (name, path relative to backup root, date, size ect.) + file hash | sposta file da una cartella all'altra sul server | C | S
DC | directory create | directory metadata (name, path relative to backup root) | crea cartella sul server | C | S
DD | directory delete | directory metadata (name, path relative to backup root) | elimina cartella sul server | C | S
DM | directory move | previous directory metadata (name, path relative to backup root) + new directory metadata (name, path relative to backup root) | sposta cartella sul server | C | S
DR | directory rename | previous directory metadata (name, path relative to backup root) + new directory metadata (name, path relative to backup root) | rinomina cartella sul server | C | S
CV | change version | latest supported version | comporta un cambiamento di versione nei messaggi inviati dall'alta party all'ultima versione supportata da questo party oppure porta alla fine dell'interazione | C/S | S/C
PF | probe file | file hash | compara file hash ricevuta con file hash possedute e rispondi al client OK o NO | C | S
OK | ok | nothing | dipende da quando viene usato | S | C
NO | not ok | nothing | dipende da quando viene usato | S | C
AU | authenticate user | username + password | verifica username e password (+salt) e autentica lo user | C | S
ER | error | error code | segnala un errore | S | C