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

# Notes

### assumptions made
* The same user may want to connect to the service from different machines so I have to manage this case.
The way I managed it is by adding to the user also the information about the MAC address of the machine used
by the user in this moment; so in authentication I send username, MAC and password; the password is unique
for the same user, but the same user may use the service from different machines (MACs) in different times
(or also at the same time). The server will have a separate folder for each (username,mac) pair.
* The folders on server should not be touched directly by anyone, so they should not be modified neither while
the service is running nor when the service is sopped; this implies I know my database (and my queue in main memory)
is actually always synced with the filesystem.
Anyway, upon connection of a user, if a change in/deletion of a user's file is detected, the corresponding
database entry will be modified/removed and a warning message will be shown; then the normal operation will proceed.
* The system keeps track of all changes to the selected folder, even when the service is not running on the
client side (I manage this through a database on the client; at startup I check the differences between the
actual content of the folder and this database and I make all corresponding changes on the server side);
the main consequence of this assumption is that if a user (on the same machine), between different runs of
the service, changes the selected folder, then the copy of the previous folder on the server will be completely
removed, and only the content of the new folder will appear on the server side.
* During the service operation, the backup procedure is always client to server, no files (nor directories) will
ever transit from server to client; so the most updated version of the folder in the client will always be sent
to the server. The user may retrieve all its backed-up files (or only its backed-up files corresponding to a
specific mac address, or to the current mac address) before starting the service; so there is a specific option to
the program that will permit the retrieval of such files from server. Only in this case files may transit from server
to client. This operation will be attempted only once at the start of the program, so if errors occur (for example
connection errors) the program will be shut down. The user may also want to start the normal service operation after
the retrieval of files was completed.
* The client side of the protocol need to be asynchronous, while the server side is synchronous. I manage this by
selecting on the read and write sockets and sending a number of messages without waiting for the server responses;
after a number of messages sent without response then I stop (I cannot have more than that number of messages sent
without a response); so I have a sliding window of messages; upon the reception of a confirmation for a message
I know, since the messages are always in order, that the first message of the window has been confirmed so I move
the sliding window and proceed to send other messages until the window is full again; upon the reception of an error
message instead I know that the first message in the window has failed so that message will be skipped.
The sliding window is there in order to make it possible to re-send all already sent (but without a server
response) messages upon connection re-establishment after a connection error, so to not miss any event.
* Both client and server have a number of possible optional parameters that can be used in order to change
their behaviour. A full list of options will be presented later and can be viewed also using option -h running
the respective programs.
* Both client and server make use of a configuration file in order to further change the behaviour of the programs.
Those files are automatically generated if not previously present, and must be modified by the user in order to make
the programs work; most of the variables that can be set have a predefined default value that will be used in case
the user did not specify them, but there are a number of REQUESTED variables that the user has to set (the programs
will actually shut down if those variables are not set or contain errors).
* Both client and server make use of one or more databases to store the persistent information that they need for
the correct operation; those databases will be generated automatically if not already present.
* The communication between client and server needs to be secured, so TLS protocol will be used. (Anyway, even the
simple and unsecure TCP connection may be set to be used, but only hard-coding the relative option since I actually
want to use it only for debugging purposes and I don't want users to be able to use it)
* The encoding of the messages is performed using the Google Protocol Buffers, so raw bytes will be sent to make the
communication as efficient as possible.
* The files and directories on server side (the backed-up ones) must have the same last write time of the original
files on client side.

### file system changes to monitor
type of change | action to execute (on the client)
------------ | -------------
creation of a file | <p>create the directory entry calculating also the file hash; then ask to the server if it has already got a copy of that file (probe); then in case it is needed send the creation command (stor) for that file (file + path + size + lastWriteTime + hash); then send all the file blocks and then receive the server response</p>
deletion of a file | send to the server the command of file remove (DELE) and then receive response
creation of a directory | send to the server the command to create the directory (MKD) with the necessary information (path, lastWriteTime); then get the server response
deletion of a directory | sent to the server the directory deletion command (RMD) which is recursive (it will also remove any file or sub-directory); then get the server response

type of change | action to execute (on the server)
------------ | -------------
creation of a file | check if I already have the file mentioned in the probe message in my filesystem; send response and if it was a SEND command (meaning I don't have that file), then receive the command stor and all file blocks, create file on filesystem, check consistency with the received information (size, hash), change the file lastWriteTime to the one sent by the client and send Error or confirmation response
deletion of a file | receive the command DELE and then check if I have that file, If I have it then delete it, otherwise just confirm the deletion (if I don't have the file it is the same as if I had it and now I deleted it); then send the response (if some errors occurred this response is an error message)
creation of a directory | receive the directory creation command (MKD) and perform the related actions (create the directory if it does not already exist and change its lastWriteTime), then send the response; given the nature of the fileSystemWatcher (recursive) first all file creation commands will be sent and then the directory creation ones so a new directory may be already present; in any case the command not only creates the directory but it is also used to change the lastWriteTime for the directory so it is always useful
deletion of a directory | receive the directory deletion command (RMD) and remove the directory from disk (it is recursive so all sub-directories and files will also be deleted); then send the response

### messages
* #### general structure
    * Google protocol buffers were used throughout this project

* #### client messages
type | meaning | content | description | effects
--- | --- | --- | --- | ---
NOOP | No operation | version, type | fake message (needed to properly use protocol buffers, the first message type needs to be a NOOP | no effects
PROB | file probe | version, type, path (relative), lastWriteTime, hash | message used to probe the existence of a file on the server side | the server will check if it already has this file and respond appropriately
STOR | file store | version, type, path (relative), fileSize, lastWriteTime, hash | message used to inform the server of the client intention to send the file blocks of the file described in this message | the server will prepare the file and accept all the file data blocks from the client
DELE | file delete | version, type, path (relative), hash | message used to delete a file from the server side | the server will remove the file corresponding to the file described in this message
MKD | make directory | version, type, path (relative), lastWriteTime | message used to create a folder on the server side and/or to change its lastWriteTime (some directories will already be present, so only their lastWriteTime will be modified) | the server will create the directory, or if already present it will only change the lastWriteTime of the directory described by this message
RMD | remove directory | version, type, path (relative) | message used to delete a folder (recursively) on the server side | the server will delete the directory described by this message (recursively) 
DATA | file data block | version, type, data (the actual data), last (if this is the last block) | message used to send a single file data block from client to server | the server will append this data to the file corresponding to the last STOR message received
AUTH | authentication | version, type, username, mac, password | message used to authenticate the client to the service | the server will use the provided information to authenticate the user to the service
RETR | retrieve user's files | version, type, mac, all (if to retrieve all the user's files or only the ones corresponding to mac) | message used to ask the server for the transfer (server -> client) of all the user's files (and directories) | the server will send all the user's files and directories to the client; if all is set, all user's files will be sent, otherwise only the user's files corresponding to the provided mac

* #### server messages
type | meaning | content | description | effects
--- | --- | --- | --- | ---
NOOP | No operation | version, type | fake message (needed to properly use protocol buffers, the first message type needs to be a NOOP | no effects
OK | ok (prev command success) | version, type, code | message used to inform the client of the successful application of the previous command (the code may be used to inform of some particular conditions like a directory deletion command of a not existant folder) | the client will proceed with the next commands (sliding window moved) 
SEND | send file | version, type, path, hash | message used by the server, responding to a PROB message, to inform the client that the server does not have the file described (in PROB) in its filesystem | the client will send the STOR message followed by a number of DATA messages (blocks of the file) 
ERR | error | version, type, code | message used to signal an error happened in the server side to the client | the client, based on the error code, skip the last message sent (sliding window) or (in case of a fatal error) return. 
VER | version change | version, type, newVersion | message used to inform the client that the previous received message was of a version not supported by the server | the client will (for now, in this version of the program) return.
MKD | make directory | version, type, path, lastWriteTime | message used to create a folder on the client side and/or to change its lastWriteTime | the client will create the directory, or if already present it will only change the lastWriteTime of the directory described by this message
STOR | file store | version, type, path, fileSize, lastWriteTime, hash | message used to inform the client of the server intention to send the file blocks of the file described in this message | the client will prepare the file and accept all the file data blocks from the server
DATA | file data block | version, type, data, last | message used to send a single file data block from server to client | the client will append this data to the file corresponding to the last STOR message received

* #### messagge structure per type
    * client messages
    
        NOOP | int32 | enum
        --- | --- | ---
        | | version | type
        
        PROB | int32 | enum | string | bytes
        --- | --- | --- | --- | ---
        | | version | type | path | hash
    
        STOR | int32 | enum | string | uint64 | string | bytes
        --- | --- | --- | --- | --- | --- | ---
        | | version | type | path | file size | last write time | hash

        DELE | int32 | enum | string | bytes
        --- | --- | --- | --- | ---
        | | version | type | path | hash
        
        MKD | int32 | enum | string | string
        --- | --- | --- | --- | ---
        | | version | type | path | last write time
        
        RMD | int32 | enum | string
        --- | --- | --- | ---
        | | version | type | path
        
        DATA | int32 | enum | bytes | bool
        --- | --- | --- | --- | ---
        | | version | type | data | last
        
        AUTH | int32 | enum | string | string | string 
        --- | --- | --- | --- | --- | ---
        | | version | type | username | mac | password
        
        RETR | int32 | enum | string | bool
        --- | --- | --- | --- | ---
        | | version | type | mac | all
    
    * server messages
    
        NOOP | int32 | enum
        --- | --- | ---
        | | version | type
        
        OK | int32 | enum | int32
        --- | --- | --- | ---
        | | version | type | code
        
        SEND | int32 | enum | string | bytes
        --- | --- | --- | --- | ---
        | | version | type | path | hash
        
        ERR | int32 | enum | int32
        --- | --- | --- | ---
        | | version | type | code
        
        VER | int32 | enum | int32 
        --- | --- | --- | ---
        | | version | type | new version
        
        MKD | int32 | enum | string | string
        --- | --- | --- | --- | ---
        | | version | type | path | last write time
        
        STOR | int32 | enum | string | uint64 | string | bytes
        --- | --- | --- | --- | --- | --- | ---
        | | version | type | path | file size | last write time | hash
        
        DATA | int32 | enum | bytes | bool
        --- | --- | --- | --- | ---
        | | version | type | data | last

* #### message exchange (async)
  *(notice: clicking on the image you can modify the scheme provided you then copy and paste the markdown)
  * ##### file create
  [![](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogQXNrIHRoZSBzZXJ2ZXIgaWYgaGUgYWxyZWFkeSBoYXMgdGhlIGZpbGVcbiAgICBDLT4-UzogUFJPQih2ZXJzaW9uIHR5cGUsIHBhdGgsIGhhc2gpXG4gICAgb3B0IFRoZSBzZXJ2ZXIgZG9lcyBub3QgaGF2ZSB0aGUgZmlsZVxuICAgICAgICBTLT4-QzogU0VORCh2ZXJzaW9uLCB0eXBlLCBwYXRoLCBoYXNoKVxuICAgICAgICBOb3RlIHJpZ2h0IG9mIEM6IHNlbmQgdGhlIGZpbGUgaW5mb1xuICAgICAgICBDLT4-UzogU1RPUih2ZXJzaW9uLCB0eXBlLCBwYXRoLCBmaWxlU2l6ZSwgbGFzdFdyaXRlVGltZSwgaGFzaClcbiAgICAgICAgbG9vcCB1bnRpbCBsYXN0PT10cnVlIChkbyBmb3IgZXZlcnkgZmlsZSBibG9jaylcbiAgICAgICAgTm90ZSByaWdodCBvZiBDOiBzZW5kIHRoZSBmaWxlIGJsb2NrXG4gICAgICAgIEMtPj5TOiBEQVRBKHZlcnNpb24sIHR5cGUsIGRhdGEsIGxhc3QpXG4gICAgICAgIGVuZFxuICAgIGVuZFxuICAgIGFsdCBUaGUgc2VydmVyIGhhcyB0aGUgZmlsZSBvciBpdCBoYXMgYmVlbiBzZW50XG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoc2VudClcbiAgICBlbHNlIHRoZXJlIHdhcyBhbiBlcnJvclxuICAgICAgICBTLT4-QzogRVJSKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChlcnJvcilcbiAgICBlbmRcbiAgICBcbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogQXNrIHRoZSBzZXJ2ZXIgaWYgaGUgYWxyZWFkeSBoYXMgdGhlIGZpbGVcbiAgICBDLT4-UzogUFJPQih2ZXJzaW9uIHR5cGUsIHBhdGgsIGhhc2gpXG4gICAgb3B0IFRoZSBzZXJ2ZXIgZG9lcyBub3QgaGF2ZSB0aGUgZmlsZVxuICAgICAgICBTLT4-QzogU0VORCh2ZXJzaW9uLCB0eXBlLCBwYXRoLCBoYXNoKVxuICAgICAgICBOb3RlIHJpZ2h0IG9mIEM6IHNlbmQgdGhlIGZpbGUgaW5mb1xuICAgICAgICBDLT4-UzogU1RPUih2ZXJzaW9uLCB0eXBlLCBwYXRoLCBmaWxlU2l6ZSwgbGFzdFdyaXRlVGltZSwgaGFzaClcbiAgICAgICAgbG9vcCB1bnRpbCBsYXN0PT10cnVlIChkbyBmb3IgZXZlcnkgZmlsZSBibG9jaylcbiAgICAgICAgTm90ZSByaWdodCBvZiBDOiBzZW5kIHRoZSBmaWxlIGJsb2NrXG4gICAgICAgIEMtPj5TOiBEQVRBKHZlcnNpb24sIHR5cGUsIGRhdGEsIGxhc3QpXG4gICAgICAgIGVuZFxuICAgIGVuZFxuICAgIGFsdCBUaGUgc2VydmVyIGhhcyB0aGUgZmlsZSBvciBpdCBoYXMgYmVlbiBzZW50XG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoc2VudClcbiAgICBlbHNlIHRoZXJlIHdhcyBhbiBlcnJvclxuICAgICAgICBTLT4-QzogRVJSKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChlcnJvcilcbiAgICBlbmRcbiAgICBcbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)
  * ##### file delete
  [![](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCBmaWxlZCBkZWxldGlvbiBtZXNzYWdlXG4gICAgQy0-PlM6IERFTEUodmVyc2lvbiwgdHlwZSwgcGF0aCwgaGFzaClcbiAgICBhbHQgaWYgdGhlIGZpbGUgaGFzIGJlZW4gZGVsZXRlZCBvciB3YXMgYWxyZWFkeSBub3QgdGhlcmVcbiAgICAgICAgUy0-PkM6IE9LKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChkZWxldGVkKVxuICAgIGVsc2UgaWYgdGhlcmUgd2FzIGFuIGVycm9yXG4gICAgICAgIFMtPj5DOiBFUlIodmVyc2lvbiwgdHlwZSwgY29kZSlcbiAgICAgICAgTm90ZSBvdmVyIEMsUzogRW5kIG9mIG1lc3NhZ2UgKGVycm9yKVxuICAgIGVuZCIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCBmaWxlZCBkZWxldGlvbiBtZXNzYWdlXG4gICAgQy0-PlM6IERFTEUodmVyc2lvbiwgdHlwZSwgcGF0aCwgaGFzaClcbiAgICBhbHQgaWYgdGhlIGZpbGUgaGFzIGJlZW4gZGVsZXRlZCBvciB3YXMgYWxyZWFkeSBub3QgdGhlcmVcbiAgICAgICAgUy0-PkM6IE9LKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChkZWxldGVkKVxuICAgIGVsc2UgaWYgdGhlcmUgd2FzIGFuIGVycm9yXG4gICAgICAgIFMtPj5DOiBFUlIodmVyc2lvbiwgdHlwZSwgY29kZSlcbiAgICAgICAgTm90ZSBvdmVyIEMsUzogRW5kIG9mIG1lc3NhZ2UgKGVycm9yKVxuICAgIGVuZCIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)
  * ##### dir create
  [![](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCB0aGUgZm9sZGVyIGNyZWF0aW9uIG1lc3NhZ2VcbiAgICBDLT4-UzogTUtEKHZlcnNpb24sIHR5cGUsIHBhdGgsIGxhc3RXcml0ZVRpbWUpXG4gICAgYWx0IGlmIHRoZSBkaXJlY3RvcnkgaGFzIGJlZW4gY3JlYXRlZC91cGRhdGVkXG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoY3JlYXRlZClcbiAgICBlbHNlIGlmIHRoZXJlIHdhcyBhbiBlcnJvclxuICAgICAgICBTLT4-QzogRVJSKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChlcnJvcilcbiAgICBlbmQiLCJtZXJtYWlkIjp7InRoZW1lIjoiZGVmYXVsdCIsInRoZW1lVmFyaWFibGVzIjp7ImJhY2tncm91bmQiOiJ3aGl0ZSIsInByaW1hcnlDb2xvciI6IiNFQ0VDRkYiLCJzZWNvbmRhcnlDb2xvciI6IiNmZmZmZGUiLCJ0ZXJ0aWFyeUNvbG9yIjoiaHNsKDgwLCAxMDAlLCA5Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5Qm9yZGVyQ29sb3IiOiJoc2woMjQwLCA2MCUsIDg2LjI3NDUwOTgwMzklKSIsInNlY29uZGFyeUJvcmRlckNvbG9yIjoiaHNsKDYwLCA2MCUsIDgzLjUyOTQxMTc2NDclKSIsInRlcnRpYXJ5Qm9yZGVyQ29sb3IiOiJoc2woODAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwicHJpbWFyeVRleHRDb2xvciI6IiMxMzEzMDAiLCJzZWNvbmRhcnlUZXh0Q29sb3IiOiIjMDAwMDIxIiwidGVydGlhcnlUZXh0Q29sb3IiOiJyZ2IoOS41MDAwMDAwMDAxLCA5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSkiLCJsaW5lQ29sb3IiOiIjMzMzMzMzIiwidGV4dENvbG9yIjoiIzMzMyIsIm1haW5Ca2ciOiIjRUNFQ0ZGIiwic2Vjb25kQmtnIjoiI2ZmZmZkZSIsImJvcmRlcjEiOiIjOTM3MERCIiwiYm9yZGVyMiI6IiNhYWFhMzMiLCJhcnJvd2hlYWRDb2xvciI6IiMzMzMzMzMiLCJmb250RmFtaWx5IjoiXCJ0cmVidWNoZXQgbXNcIiwgdmVyZGFuYSwgYXJpYWwiLCJmb250U2l6ZSI6IjE2cHgiLCJsYWJlbEJhY2tncm91bmQiOiIjZThlOGU4Iiwibm9kZUJrZyI6IiNFQ0VDRkYiLCJub2RlQm9yZGVyIjoiIzkzNzBEQiIsImNsdXN0ZXJCa2ciOiIjZmZmZmRlIiwiY2x1c3RlckJvcmRlciI6IiNhYWFhMzMiLCJkZWZhdWx0TGlua0NvbG9yIjoiIzMzMzMzMyIsInRpdGxlQ29sb3IiOiIjMzMzIiwiZWRnZUxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJhY3RvckJvcmRlciI6ImhzbCgyNTkuNjI2MTY4MjI0MywgNTkuNzc2NTM2MzEyOCUsIDg3LjkwMTk2MDc4NDMlKSIsImFjdG9yQmtnIjoiI0VDRUNGRiIsImFjdG9yVGV4dENvbG9yIjoiYmxhY2siLCJhY3RvckxpbmVDb2xvciI6ImdyZXkiLCJzaWduYWxDb2xvciI6IiMzMzMiLCJzaWduYWxUZXh0Q29sb3IiOiIjMzMzIiwibGFiZWxCb3hCa2dDb2xvciI6IiNFQ0VDRkYiLCJsYWJlbEJveEJvcmRlckNvbG9yIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwibGFiZWxUZXh0Q29sb3IiOiJibGFjayIsImxvb3BUZXh0Q29sb3IiOiJibGFjayIsIm5vdGVCb3JkZXJDb2xvciI6IiNhYWFhMzMiLCJub3RlQmtnQ29sb3IiOiIjZmZmNWFkIiwibm90ZVRleHRDb2xvciI6ImJsYWNrIiwiYWN0aXZhdGlvbkJvcmRlckNvbG9yIjoiIzY2NiIsImFjdGl2YXRpb25Ca2dDb2xvciI6IiNmNGY0ZjQiLCJzZXF1ZW5jZU51bWJlckNvbG9yIjoid2hpdGUiLCJzZWN0aW9uQmtnQ29sb3IiOiJyZ2JhKDEwMiwgMTAyLCAyNTUsIDAuNDkpIiwiYWx0U2VjdGlvbkJrZ0NvbG9yIjoid2hpdGUiLCJzZWN0aW9uQmtnQ29sb3IyIjoiI2ZmZjQwMCIsInRhc2tCb3JkZXJDb2xvciI6IiM1MzRmYmMiLCJ0YXNrQmtnQ29sb3IiOiIjOGE5MGRkIiwidGFza1RleHRMaWdodENvbG9yIjoid2hpdGUiLCJ0YXNrVGV4dENvbG9yIjoid2hpdGUiLCJ0YXNrVGV4dERhcmtDb2xvciI6ImJsYWNrIiwidGFza1RleHRPdXRzaWRlQ29sb3IiOiJibGFjayIsInRhc2tUZXh0Q2xpY2thYmxlQ29sb3IiOiIjMDAzMTYzIiwiYWN0aXZlVGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsImFjdGl2ZVRhc2tCa2dDb2xvciI6IiNiZmM3ZmYiLCJncmlkQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JrZ0NvbG9yIjoibGlnaHRncmV5IiwiZG9uZVRhc2tCb3JkZXJDb2xvciI6ImdyZXkiLCJjcml0Qm9yZGVyQ29sb3IiOiIjZmY4ODg4IiwiY3JpdEJrZ0NvbG9yIjoicmVkIiwidG9kYXlMaW5lQ29sb3IiOiJyZWQiLCJsYWJlbENvbG9yIjoiYmxhY2siLCJlcnJvckJrZ0NvbG9yIjoiIzU1MjIyMiIsImVycm9yVGV4dENvbG9yIjoiIzU1MjIyMiIsImNsYXNzVGV4dCI6IiMxMzEzMDAiLCJmaWxsVHlwZTAiOiIjRUNFQ0ZGIiwiZmlsbFR5cGUxIjoiI2ZmZmZkZSIsImZpbGxUeXBlMiI6ImhzbCgzMDQsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsImZpbGxUeXBlMyI6ImhzbCgxMjQsIDEwMCUsIDkzLjUyOTQxMTc2NDclKSIsImZpbGxUeXBlNCI6ImhzbCgxNzYsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsImZpbGxUeXBlNSI6ImhzbCgtNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU2IjoiaHNsKDgsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsImZpbGxUeXBlNyI6ImhzbCgxODgsIDEwMCUsIDkzLjUyOTQxMTc2NDclKSJ9fSwidXBkYXRlRWRpdG9yIjpmYWxzZX0)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCB0aGUgZm9sZGVyIGNyZWF0aW9uIG1lc3NhZ2VcbiAgICBDLT4-UzogTUtEKHZlcnNpb24sIHR5cGUsIHBhdGgsIGxhc3RXcml0ZVRpbWUpXG4gICAgYWx0IGlmIHRoZSBkaXJlY3RvcnkgaGFzIGJlZW4gY3JlYXRlZC91cGRhdGVkXG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoY3JlYXRlZClcbiAgICBlbHNlIGlmIHRoZXJlIHdhcyBhbiBlcnJvclxuICAgICAgICBTLT4-QzogRVJSKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChlcnJvcilcbiAgICBlbmQiLCJtZXJtYWlkIjp7InRoZW1lIjoiZGVmYXVsdCIsInRoZW1lVmFyaWFibGVzIjp7ImJhY2tncm91bmQiOiJ3aGl0ZSIsInByaW1hcnlDb2xvciI6IiNFQ0VDRkYiLCJzZWNvbmRhcnlDb2xvciI6IiNmZmZmZGUiLCJ0ZXJ0aWFyeUNvbG9yIjoiaHNsKDgwLCAxMDAlLCA5Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5Qm9yZGVyQ29sb3IiOiJoc2woMjQwLCA2MCUsIDg2LjI3NDUwOTgwMzklKSIsInNlY29uZGFyeUJvcmRlckNvbG9yIjoiaHNsKDYwLCA2MCUsIDgzLjUyOTQxMTc2NDclKSIsInRlcnRpYXJ5Qm9yZGVyQ29sb3IiOiJoc2woODAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwicHJpbWFyeVRleHRDb2xvciI6IiMxMzEzMDAiLCJzZWNvbmRhcnlUZXh0Q29sb3IiOiIjMDAwMDIxIiwidGVydGlhcnlUZXh0Q29sb3IiOiJyZ2IoOS41MDAwMDAwMDAxLCA5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSkiLCJsaW5lQ29sb3IiOiIjMzMzMzMzIiwidGV4dENvbG9yIjoiIzMzMyIsIm1haW5Ca2ciOiIjRUNFQ0ZGIiwic2Vjb25kQmtnIjoiI2ZmZmZkZSIsImJvcmRlcjEiOiIjOTM3MERCIiwiYm9yZGVyMiI6IiNhYWFhMzMiLCJhcnJvd2hlYWRDb2xvciI6IiMzMzMzMzMiLCJmb250RmFtaWx5IjoiXCJ0cmVidWNoZXQgbXNcIiwgdmVyZGFuYSwgYXJpYWwiLCJmb250U2l6ZSI6IjE2cHgiLCJsYWJlbEJhY2tncm91bmQiOiIjZThlOGU4Iiwibm9kZUJrZyI6IiNFQ0VDRkYiLCJub2RlQm9yZGVyIjoiIzkzNzBEQiIsImNsdXN0ZXJCa2ciOiIjZmZmZmRlIiwiY2x1c3RlckJvcmRlciI6IiNhYWFhMzMiLCJkZWZhdWx0TGlua0NvbG9yIjoiIzMzMzMzMyIsInRpdGxlQ29sb3IiOiIjMzMzIiwiZWRnZUxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJhY3RvckJvcmRlciI6ImhzbCgyNTkuNjI2MTY4MjI0MywgNTkuNzc2NTM2MzEyOCUsIDg3LjkwMTk2MDc4NDMlKSIsImFjdG9yQmtnIjoiI0VDRUNGRiIsImFjdG9yVGV4dENvbG9yIjoiYmxhY2siLCJhY3RvckxpbmVDb2xvciI6ImdyZXkiLCJzaWduYWxDb2xvciI6IiMzMzMiLCJzaWduYWxUZXh0Q29sb3IiOiIjMzMzIiwibGFiZWxCb3hCa2dDb2xvciI6IiNFQ0VDRkYiLCJsYWJlbEJveEJvcmRlckNvbG9yIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwibGFiZWxUZXh0Q29sb3IiOiJibGFjayIsImxvb3BUZXh0Q29sb3IiOiJibGFjayIsIm5vdGVCb3JkZXJDb2xvciI6IiNhYWFhMzMiLCJub3RlQmtnQ29sb3IiOiIjZmZmNWFkIiwibm90ZVRleHRDb2xvciI6ImJsYWNrIiwiYWN0aXZhdGlvbkJvcmRlckNvbG9yIjoiIzY2NiIsImFjdGl2YXRpb25Ca2dDb2xvciI6IiNmNGY0ZjQiLCJzZXF1ZW5jZU51bWJlckNvbG9yIjoid2hpdGUiLCJzZWN0aW9uQmtnQ29sb3IiOiJyZ2JhKDEwMiwgMTAyLCAyNTUsIDAuNDkpIiwiYWx0U2VjdGlvbkJrZ0NvbG9yIjoid2hpdGUiLCJzZWN0aW9uQmtnQ29sb3IyIjoiI2ZmZjQwMCIsInRhc2tCb3JkZXJDb2xvciI6IiM1MzRmYmMiLCJ0YXNrQmtnQ29sb3IiOiIjOGE5MGRkIiwidGFza1RleHRMaWdodENvbG9yIjoid2hpdGUiLCJ0YXNrVGV4dENvbG9yIjoid2hpdGUiLCJ0YXNrVGV4dERhcmtDb2xvciI6ImJsYWNrIiwidGFza1RleHRPdXRzaWRlQ29sb3IiOiJibGFjayIsInRhc2tUZXh0Q2xpY2thYmxlQ29sb3IiOiIjMDAzMTYzIiwiYWN0aXZlVGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsImFjdGl2ZVRhc2tCa2dDb2xvciI6IiNiZmM3ZmYiLCJncmlkQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JrZ0NvbG9yIjoibGlnaHRncmV5IiwiZG9uZVRhc2tCb3JkZXJDb2xvciI6ImdyZXkiLCJjcml0Qm9yZGVyQ29sb3IiOiIjZmY4ODg4IiwiY3JpdEJrZ0NvbG9yIjoicmVkIiwidG9kYXlMaW5lQ29sb3IiOiJyZWQiLCJsYWJlbENvbG9yIjoiYmxhY2siLCJlcnJvckJrZ0NvbG9yIjoiIzU1MjIyMiIsImVycm9yVGV4dENvbG9yIjoiIzU1MjIyMiIsImNsYXNzVGV4dCI6IiMxMzEzMDAiLCJmaWxsVHlwZTAiOiIjRUNFQ0ZGIiwiZmlsbFR5cGUxIjoiI2ZmZmZkZSIsImZpbGxUeXBlMiI6ImhzbCgzMDQsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsImZpbGxUeXBlMyI6ImhzbCgxMjQsIDEwMCUsIDkzLjUyOTQxMTc2NDclKSIsImZpbGxUeXBlNCI6ImhzbCgxNzYsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsImZpbGxUeXBlNSI6ImhzbCgtNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU2IjoiaHNsKDgsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsImZpbGxUeXBlNyI6ImhzbCgxODgsIDEwMCUsIDkzLjUyOTQxMTc2NDclKSJ9fSwidXBkYXRlRWRpdG9yIjpmYWxzZX0)
  * ##### dir delete
  [![](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCB0aGUgZm9sZGVyIGRlbGV0aW9uIG1lc3NhZ2VcbiAgICBDLT4-UzogUk1EKHZlcnNpb24sIHR5cGUsIHBhdGgpXG4gICAgYWx0IGlmIHRoZSBkaXJlY3RvcnkgaGFzIGJlZW4gZGVsZXRlZCBvciBpdCB3YXMgbm90IGFscmVhZHkgdGhlcmVcbiAgICAgICAgUy0-PkM6IE9LKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChyZW1vdmVkKVxuICAgIGVsc2UgaWYgdGhlcmUgd2FzIGFuIGVycm9yXG4gICAgICAgIFMtPj5DOiBFUlIodmVyc2lvbiwgdHlwZSwgY29kZSlcbiAgICAgICAgTm90ZSBvdmVyIEMsUzogRW5kIG9mIG1lc3NhZ2UgKGVycm9yKVxuICAgIGVuZCIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCB0aGUgZm9sZGVyIGRlbGV0aW9uIG1lc3NhZ2VcbiAgICBDLT4-UzogUk1EKHZlcnNpb24sIHR5cGUsIHBhdGgpXG4gICAgYWx0IGlmIHRoZSBkaXJlY3RvcnkgaGFzIGJlZW4gZGVsZXRlZCBvciBpdCB3YXMgbm90IGFscmVhZHkgdGhlcmVcbiAgICAgICAgUy0-PkM6IE9LKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChyZW1vdmVkKVxuICAgIGVsc2UgaWYgdGhlcmUgd2FzIGFuIGVycm9yXG4gICAgICAgIFMtPj5DOiBFUlIodmVyc2lvbiwgdHlwZSwgY29kZSlcbiAgICAgICAgTm90ZSBvdmVyIEMsUzogRW5kIG9mIG1lc3NhZ2UgKGVycm9yKVxuICAgIGVuZCIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)
  * ##### user's file retrieve
  [![](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogU2VuZCB0byBzZXJ2ZXIgdGhlIFJFVFIgbWVzc2FnZVxuICAgIEMtPj5TOiBSRVRSKHZlcnNpb24sIHR5cGUsIG1hYywgYWxsKVxuICAgIGFsdCBUaGUgc2VydmVyIHNlbmRzIGFsbCBmaWxlcyB0byB1c2VyXG4gICAgICAgIGxvb3AgZm9yIGVhY2ggdXNlcidzIGZpbGUvZGlyZWN0b3J5IHNlbGVjdGVkXG4gICAgICAgICAgICBTLT4-QzogU1RPUih2ZXJzaW9uLCB0eXBlLCBwYXRoLCBmaWxlU2l6ZSwgbGFzdFdyaXRlVGltZSwgaGFzaClcbiAgICAgICAgICAgIGxvb3AgdW50aWwgbGFzdD09dHJ1ZSAoZG8gZm9yIGV2ZXJ5IGZpbGUgYmxvY2spXG4gICAgICAgICAgICBOb3RlIHJpZ2h0IG9mIEM6IHNlbmQgdGhlIGZpbGUgYmxvY2tcbiAgICAgICAgICAgIFMtPj5DOiBEQVRBKHZlcnNpb24sIHR5cGUsIGRhdGEsIGxhc3QpXG4gICAgICAgICAgICBlbmRcbiAgICAgICAgZW5kXG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoc2VudClcbiAgICBlbHNlIHRoZXJlIHdhcyBhbiBlcnJvclxuICAgICAgICBTLT4-QzogRVJSKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChlcnJvcilcbiAgICBlbmRcbiAgICBcbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogU2VuZCB0byBzZXJ2ZXIgdGhlIFJFVFIgbWVzc2FnZVxuICAgIEMtPj5TOiBSRVRSKHZlcnNpb24sIHR5cGUsIG1hYywgYWxsKVxuICAgIGFsdCBUaGUgc2VydmVyIHNlbmRzIGFsbCBmaWxlcyB0byB1c2VyXG4gICAgICAgIGxvb3AgZm9yIGVhY2ggdXNlcidzIGZpbGUvZGlyZWN0b3J5IHNlbGVjdGVkXG4gICAgICAgICAgICBTLT4-QzogU1RPUih2ZXJzaW9uLCB0eXBlLCBwYXRoLCBmaWxlU2l6ZSwgbGFzdFdyaXRlVGltZSwgaGFzaClcbiAgICAgICAgICAgIGxvb3AgdW50aWwgbGFzdD09dHJ1ZSAoZG8gZm9yIGV2ZXJ5IGZpbGUgYmxvY2spXG4gICAgICAgICAgICBOb3RlIHJpZ2h0IG9mIEM6IHNlbmQgdGhlIGZpbGUgYmxvY2tcbiAgICAgICAgICAgIFMtPj5DOiBEQVRBKHZlcnNpb24sIHR5cGUsIGRhdGEsIGxhc3QpXG4gICAgICAgICAgICBlbmRcbiAgICAgICAgZW5kXG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoc2VudClcbiAgICBlbHNlIHRoZXJlIHdhcyBhbiBlcnJvclxuICAgICAgICBTLT4-QzogRVJSKHZlcnNpb24sIHR5cGUsIGNvZGUpXG4gICAgICAgIE5vdGUgb3ZlciBDLFM6IEVuZCBvZiBtZXNzYWdlIChlcnJvcilcbiAgICBlbmRcbiAgICBcbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)
  * ##### user authentication
  [![](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCB0aGUgYXV0aCBtZXNzYWdlXG4gICAgQy0-PlM6IEFVVEgodmVyc2lvbiwgdHlwZSwgdXNlcm5hbWUsIG1hYywgcGFzc3dvcmQpXG4gICAgYWx0IGlmIHRoZSBhdXRoZW50aWNhdGlvbiBpcyBzdWNjZXNzZnVsXG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoYXV0aGVudGljYXRlZClcbiAgICBlbHNlIGlmIHRoZXJlIHdhcyBhbiBlcnJvciAoYm90aCBwcm90b2NvbCBhbmQgYXV0aClcbiAgICAgICAgUy0-PkM6IEVSUih2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoZXJyb3IpXG4gICAgZWxzZSBpZiB0aGUgc2VydmVyIHJlcXVpcmVzIGEgdmVyc2lvbiBjaGFuZ2VcbiAgICAgICAgUy0-PkM6IFZFUih2ZXJzaW9uLCB0eXBlLCBuZXdWZXJzaW9uKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoY2hhbmdlIHZlcnNpb24pXG4gICAgZW5kXG5cbiAgICBcbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgcGFydGljaXBhbnQgQyBhcyBDbGllbnRcbiAgICBwYXJ0aWNpcGFudCBTIGFzIFNlcnZlclxuICAgIE5vdGUgcmlnaHQgb2YgQzogc2VuZCB0aGUgYXV0aCBtZXNzYWdlXG4gICAgQy0-PlM6IEFVVEgodmVyc2lvbiwgdHlwZSwgdXNlcm5hbWUsIG1hYywgcGFzc3dvcmQpXG4gICAgYWx0IGlmIHRoZSBhdXRoZW50aWNhdGlvbiBpcyBzdWNjZXNzZnVsXG4gICAgICAgIFMtPj5DOiBPSyh2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoYXV0aGVudGljYXRlZClcbiAgICBlbHNlIGlmIHRoZXJlIHdhcyBhbiBlcnJvciAoYm90aCBwcm90b2NvbCBhbmQgYXV0aClcbiAgICAgICAgUy0-PkM6IEVSUih2ZXJzaW9uLCB0eXBlLCBjb2RlKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoZXJyb3IpXG4gICAgZWxzZSBpZiB0aGUgc2VydmVyIHJlcXVpcmVzIGEgdmVyc2lvbiBjaGFuZ2VcbiAgICAgICAgUy0-PkM6IFZFUih2ZXJzaW9uLCB0eXBlLCBuZXdWZXJzaW9uKVxuICAgICAgICBOb3RlIG92ZXIgQyxTOiBFbmQgb2YgbWVzc2FnZSAoY2hhbmdlIHZlcnNpb24pXG4gICAgZW5kXG5cbiAgICBcbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0IiwidGhlbWVWYXJpYWJsZXMiOnsiYmFja2dyb3VuZCI6IndoaXRlIiwicHJpbWFyeUNvbG9yIjoiI0VDRUNGRiIsInNlY29uZGFyeUNvbG9yIjoiI2ZmZmZkZSIsInRlcnRpYXJ5Q29sb3IiOiJoc2woODAsIDEwMCUsIDk2LjI3NDUwOTgwMzklKSIsInByaW1hcnlCb3JkZXJDb2xvciI6ImhzbCgyNDAsIDYwJSwgODYuMjc0NTA5ODAzOSUpIiwic2Vjb25kYXJ5Qm9yZGVyQ29sb3IiOiJoc2woNjAsIDYwJSwgODMuNTI5NDExNzY0NyUpIiwidGVydGlhcnlCb3JkZXJDb2xvciI6ImhzbCg4MCwgNjAlLCA4Ni4yNzQ1MDk4MDM5JSkiLCJwcmltYXJ5VGV4dENvbG9yIjoiIzEzMTMwMCIsInNlY29uZGFyeVRleHRDb2xvciI6IiMwMDAwMjEiLCJ0ZXJ0aWFyeVRleHRDb2xvciI6InJnYig5LjUwMDAwMDAwMDEsIDkuNTAwMDAwMDAwMSwgOS41MDAwMDAwMDAxKSIsImxpbmVDb2xvciI6IiMzMzMzMzMiLCJ0ZXh0Q29sb3IiOiIjMzMzIiwibWFpbkJrZyI6IiNFQ0VDRkYiLCJzZWNvbmRCa2ciOiIjZmZmZmRlIiwiYm9yZGVyMSI6IiM5MzcwREIiLCJib3JkZXIyIjoiI2FhYWEzMyIsImFycm93aGVhZENvbG9yIjoiIzMzMzMzMyIsImZvbnRGYW1pbHkiOiJcInRyZWJ1Y2hldCBtc1wiLCB2ZXJkYW5hLCBhcmlhbCIsImZvbnRTaXplIjoiMTZweCIsImxhYmVsQmFja2dyb3VuZCI6IiNlOGU4ZTgiLCJub2RlQmtnIjoiI0VDRUNGRiIsIm5vZGVCb3JkZXIiOiIjOTM3MERCIiwiY2x1c3RlckJrZyI6IiNmZmZmZGUiLCJjbHVzdGVyQm9yZGVyIjoiI2FhYWEzMyIsImRlZmF1bHRMaW5rQ29sb3IiOiIjMzMzMzMzIiwidGl0bGVDb2xvciI6IiMzMzMiLCJlZGdlTGFiZWxCYWNrZ3JvdW5kIjoiI2U4ZThlOCIsImFjdG9yQm9yZGVyIjoiaHNsKDI1OS42MjYxNjgyMjQzLCA1OS43NzY1MzYzMTI4JSwgODcuOTAxOTYwNzg0MyUpIiwiYWN0b3JCa2ciOiIjRUNFQ0ZGIiwiYWN0b3JUZXh0Q29sb3IiOiJibGFjayIsImFjdG9yTGluZUNvbG9yIjoiZ3JleSIsInNpZ25hbENvbG9yIjoiIzMzMyIsInNpZ25hbFRleHRDb2xvciI6IiMzMzMiLCJsYWJlbEJveEJrZ0NvbG9yIjoiI0VDRUNGRiIsImxhYmVsQm94Qm9yZGVyQ29sb3IiOiJoc2woMjU5LjYyNjE2ODIyNDMsIDU5Ljc3NjUzNjMxMjglLCA4Ny45MDE5NjA3ODQzJSkiLCJsYWJlbFRleHRDb2xvciI6ImJsYWNrIiwibG9vcFRleHRDb2xvciI6ImJsYWNrIiwibm90ZUJvcmRlckNvbG9yIjoiI2FhYWEzMyIsIm5vdGVCa2dDb2xvciI6IiNmZmY1YWQiLCJub3RlVGV4dENvbG9yIjoiYmxhY2siLCJhY3RpdmF0aW9uQm9yZGVyQ29sb3IiOiIjNjY2IiwiYWN0aXZhdGlvbkJrZ0NvbG9yIjoiI2Y0ZjRmNCIsInNlcXVlbmNlTnVtYmVyQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvciI6InJnYmEoMTAyLCAxMDIsIDI1NSwgMC40OSkiLCJhbHRTZWN0aW9uQmtnQ29sb3IiOiJ3aGl0ZSIsInNlY3Rpb25Ca2dDb2xvcjIiOiIjZmZmNDAwIiwidGFza0JvcmRlckNvbG9yIjoiIzUzNGZiYyIsInRhc2tCa2dDb2xvciI6IiM4YTkwZGQiLCJ0YXNrVGV4dExpZ2h0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0Q29sb3IiOiJ3aGl0ZSIsInRhc2tUZXh0RGFya0NvbG9yIjoiYmxhY2siLCJ0YXNrVGV4dE91dHNpZGVDb2xvciI6ImJsYWNrIiwidGFza1RleHRDbGlja2FibGVDb2xvciI6IiMwMDMxNjMiLCJhY3RpdmVUYXNrQm9yZGVyQ29sb3IiOiIjNTM0ZmJjIiwiYWN0aXZlVGFza0JrZ0NvbG9yIjoiI2JmYzdmZiIsImdyaWRDb2xvciI6ImxpZ2h0Z3JleSIsImRvbmVUYXNrQmtnQ29sb3IiOiJsaWdodGdyZXkiLCJkb25lVGFza0JvcmRlckNvbG9yIjoiZ3JleSIsImNyaXRCb3JkZXJDb2xvciI6IiNmZjg4ODgiLCJjcml0QmtnQ29sb3IiOiJyZWQiLCJ0b2RheUxpbmVDb2xvciI6InJlZCIsImxhYmVsQ29sb3IiOiJibGFjayIsImVycm9yQmtnQ29sb3IiOiIjNTUyMjIyIiwiZXJyb3JUZXh0Q29sb3IiOiIjNTUyMjIyIiwiY2xhc3NUZXh0IjoiIzEzMTMwMCIsImZpbGxUeXBlMCI6IiNFQ0VDRkYiLCJmaWxsVHlwZTEiOiIjZmZmZmRlIiwiZmlsbFR5cGUyIjoiaHNsKDMwNCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGUzIjoiaHNsKDEyNCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIiwiZmlsbFR5cGU0IjoiaHNsKDE3NiwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU1IjoiaHNsKC00LCAxMDAlLCA5My41Mjk0MTE3NjQ3JSkiLCJmaWxsVHlwZTYiOiJoc2woOCwgMTAwJSwgOTYuMjc0NTA5ODAzOSUpIiwiZmlsbFR5cGU3IjoiaHNsKDE4OCwgMTAwJSwgOTMuNTI5NDExNzY0NyUpIn19LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)

### client structure
* The client has an event queue which is a thread safe circular vector of events; events are the representation of a change
to a directory entry (element of the filesystem) (so it will bind a directory entry to a kind of event (creation, modification, deletion)).
This event queue will be filled by the filesystem watcher and it will be emptied by the actual communication thread (talking with the server)
* The client has 1 main thread in polling (every x seconds) on the directory to watch (file system watcher), which at every change detected
will add an event (indication of that change) to the event queue; changes will be detected comparing the actual situation of the
folder to be watched with the _paths map which is an in main memory representation of the folder.
* The client has a database to which he saves the current state of the folder to watch and it is used at startup to fill the content of the
_paths map (in main memory representation of the folder to watch) so that changes while the program is stopped will be detected.
* The client gets its configuration (all variables needed for the execution of the client) from a configuration file, so there 
is a config class which does just that.
* Finally the client has a communication thread talking with the server; this thread will send messages corresponding to the changes it
can get from the event queue; it will select on the read and write sockets (so to make this protocol asynchronous) and will use
a protocol manager to actually send, receive, interpret messages; the protocol manager has a sliding window of messages which
can be sent without the server confirmation; in case of errors the protocol manager will skip the faulty message; in case of
connection errors the protocol manager will instead try to re-send all waiting messages; in case there are no communications for a certain amount of time
the client will disconnect from the server; in case of socket errors (in communication) the client will re-try for a limited
number of times or unlimitedly (if the corresponding option is set).

### server structure
* The server has 1 main thread which accepts connections from clients and dispatch them to the (fixed) thread pool
* The server has a configuration file (and so a configuration object) from which to get the configuration for its run (all the variables needed).
* The server has 2 databases: one database of directory entries similar to the one used by the client but that contains also the information about
the username and mac related to each single directory entry so as to read/modify/delete only entries related to the user,mac connected; and one
database of passwords which contains (username,salt,hash) tuples in order to be able to authenticate a user.
* The server has a fixed sized thread pool of singleServer threads which will get a socket each from a queue of sockets and communicate
with a single client (the one connected on that socket); this thread will first authenticate the client connected and then will select
on the read socket so that to receive the client commands; then it will use a protocol manager to manage each single message from
the client and to apply its commands; in case of socket errors or in case of lack of messages for a certain amount of time
the single thread will take another socket and continue; errors in messages will lead to nothing changed on the server side
and to the sending of error messages back to the client.

### main option arguments
#### client side
    NAME
        PDS_BACKUP client
    
    SYNOPSIS
        programName [--help]
            [--retrieve destFolder] [--mac macAddress] [--all] [--start]
            [--ip server_ipaddress] [--port server_port] [--user username] [--pass password]
    
    OPTIONS
        --help (abbr -h)
            Print out a usage message
    
        --retrieve (abbr -r)
            Requests the server (after authentication) to send to the client the copy of the folders and files of
            the specified user. The data will be put in the specified [destDir]. If no other commands are specified
            (no --mac, no --all) then only the files and directories for the current mac address will be retrieved.
            This command requires the presence of the following other commands: [--ip] [--port] [--user] [--pass] [--dir]
    
        --dir (abbr -m) destDir
            Sets the [destDir] of the user's data to retrieve.
            Needed by --retrieve.
    
        --mac (abbr -m) macAddress
            Sets the [macAddress] of the user's data to retrieve.
            To be used with --retrieve.
    
        --all (abbr -a)
            Specifies to retrieve all user's data,
             To be used with --retrieve.
    
        --start (abbr -s)
            Start the server (if not present the server will stop after having created/loaded the Config file).
             This command requires the presence of the following other commands: [--ip] [--port] [--user] [--pass]
    
        --ip (abbr -i) server_ipaddress
            Sets the [ip] address of the server to contact.
             Needed by --start and --retrieve.
    
        --port (abbr -p) server_port
            Sets the [port] of the server to contact.
             Needed by --start and --retrieve.
    
        --user (abbr -u) username
            Sets the [username] to use to authenticate to the server.
            Needed by --start and --retrieve.
    
        --pass (abbr -w) password
            Sets the [password] to use to authenticate to the server.
            Needed by --start and --retrieve.
    
        --persist (abbr -t)
            If connection is lost, keep trying indefinitely.

#### server side
    NAME
        PDS_BACKUP server
    
    SYNOPSIS
        programName [--help]
            [--addU username] [--updateU username] [--removeU username] [--viewU]
            [--pass password] [--delete username] [--mac macAddress] [--start]
    
    OPTIONS
        --help (abbr -h)
            Print out a usage message
    
        --addU (abbr -a) username
            Add the user with [username] to the server, the option --pass (-p) is needed to set the user password.
            This option is mutually exclusive with --updateU and --removeU.
    
        --updateU (abbr -u) username
            Update the user with [username] to the server, the option --pass (-p) is needed to set the new user password.
            This option is mutually exclusive with --addU and --removeU.
    
        --removeU (abbr -r) username
            Remove the user with [username] (and all its backed up data) from the server.
            This option is mutually exclusive with --addU and --removeU.
    
        --viewU (abbr -v)
            Print all the username of all registered users.
    
        --pass (abbr -p) password
            Set the [password] to use.
            This option is needed by the options --addU and --updateU.
    
        --delete (abbr -d) username
            Makes the server delete all or one of the specified [username] backups before (optionally) starting the service.
            If no other options (no --mac) are specified then it will remove all the user's backups from server.
    
        --mac (abbr -m) macAddress
            Specifies the [macAddress] for the --delete option.
            If this option is set the --delete option will only delete the user's backup related to this [macAddress].
    
        --start (abbr -s)
            Start the server

### configuration file parameters
#### client side
    ###########################################################################
    #        -Host specific variables: no default values are provided-        #
    ###########################################################################
    
    # Path of the folder to back up on server
    path_to_watch = C:\Users\michele\Desktop\PDS_backup\client_path_to_watch
    
    
    ###########################################################################
    #                             Other variables                             #
    #        -  in case of empty fields default values will be used  -        #
    ###########################################################################
    
    # Client Database path
    database_path = ../clientFiles/clientDB.sqlite
    
    # CA to use for server certificate verification
    ca_file_path = ../../TLScerts/cacert.pem
    
    # Milliseconds the file system watcher between one folder (to watch) polling and the other
    millis_filesystem_watcher = 5000
    
    # Maximum size for the event queue (in practice how many events can be detected before sending them to server)
    event_queue_size = 20
    
    # Seconds the client will wait between one connection attempt and the other
    seconds_between_reconnections = 10
    
    # Maximum number of allowed connection attempts
    max_connection_retries = 6
    
    # Seconds to wait before the client will disconnect
    timeout_seconds = 15
    
    # Seconds the client will wait between 2 subsequent selects on the socket
    select_timeout_seconds = 5
    
    # Maximum number of messages waiting for a server response allowed
    max_response_waiting = 1024
    
    # Temporary files name size
    tmp_file_name_size = 8
    
    # Maximum size (in bytes) of the file transfer chunks ('data' part of DATA messages)
    # the maximum size for a protocol buffer message is 64MB, for a TCP socket it is 1GB,
    # and for a TLS socket it is 16KB.
    # So, keeping in mind that there are also other fields in the message,
    # KEEP IT BELOW (or equal) 15KB.
    max_data_chunk_size = 15360

#### server side
    # Server base folder path (where user files will be saved)
    server_base_path = C:\Users\michele\Desktop\PDS_backup\server_base_path
    
    # Temporary folder path for temporary files
    temp_path = C:\Users\michele\Desktop\PDS_backup\server_base_path\temp
    
    
    ###########################################################################
    #                             Other variables                             #
    #        -  in case of empty fields default values will be used  -        #
    ###########################################################################
    
    # Password Database path
    password_database_path = ../serverFiles/passwordDB.sqlite
    
    # Server Database path
    server_database_path = ../serverFiles/serverDB.sqlite
    
    # Server Certificate path
    certificate_path = ../../TLScerts/server_cert.pem
    
    # Server Private Key path
    private_key_path = ../../TLScerts/server_pkey.pem
    
    # CA to use for server certificate verification
    ca_file_path = ../../TLScerts/cacert.pem
    
    # Size of the accept listen queue
    listen_queue = 8
    
    # Number of single server threads (apart from the accepting thread)
    n_threads = 4
    
    # Maximum socket queue size
    socket_queue_size = 10
    
    # Seconds the server will wait between 2 subsequent selects on the socket
    select_timeout_seconds = 5
    
    # Seconds the server will wait before disconnecting client
    timeout_seconds = 60
    
    # Temporary files name size
    tmp_file_name_size = 8
    
    # Maximum size (in bytes) of the file transfer chunks ('data' part of DATA messages)
    # the maximum size for a protocol buffer message is 64MB, for a TCP socket it is 1GB,
    # and for a TLS socket it is 16KB.
    # So, keeping in mind that there are also other fields in the message,
    # KEEP IT BELOW (or equal) 15KB.
    max_data_chunk_size = 15360

### implemented classes (and brief description)
#### client side
* <b>main</b>
* <b>ArgumentsManager</b> class; used to manage the user option arguments to client main
* <b>Config</b> class; used to load and manage the configuration file parameters for the server
* <b>Database</b> class; used to manage the client database
* <b>Event</b> class; used to represent a filesystem event
* <b>FileSystemWatcher</b> class; used to watch a folder for changes and update the list of events
* <b>ProtocolManager</b> class; used to manage the communication with server (protocol)
* <b>Thread_guard</b> class; used to manage the correct closing of the client program

#### server side
* <b>main</b>
* <b>ArgumentsManger</b> class; used to manage the user option arguments to server main
* <b>Config</b> class; used to load and manage the configuration file parameters for the server
* <b>Database</b> class; used to manage the server database
* <b>Database_pwd</b> class; used to manage the server password database
* <b>ProtocolManager</b> class; used to manage the communication with client (protocol)
* <b>Thread_guard</b> class; used to manage the correct closing of the server program

#### library
* <b>Circular_vector</b> class; implements both a thread safe and a simple circular vector
* <b>Directory_entry</b> class; used to represent a directory element (and to calculate its hash and read/modify its lastWriteTime)
* <b>Hash</b> class; used to calculate hashes of strings or generic data
* <b>Message</b> class; used to show (in a thread safe way) messages in a predefined format
* <b>RandomNumberGenerator</b> class; used to generate random numbers and strings (for salt and random names)
* <b>Socket</b> class; implements both TCP and TLS connections
* <b>Validator</b> class; used to validate user input or server/client messages

# configuration notes
## on linux (ubuntu):
* install gcc 9 (gcc 8 may also work but 9 is better):
  * ``sudo apt install build-essential``
  * ``gcc --version`` -> check the version is actually 9
  * if it is not version 9 then to this: (installation of multiple configurations (or just install only version 9))
  * ``sudo apt install software-properties-common``
  * ``sudo add-apt-repository ppa:ubuntu-toolchain-r/test``
  * ``sudo apt install gcc-7 g++-7 gcc-8 g++-8 gcc-9 g++-9``
  ```
       sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90 --slave /usr/bin/g++ g++ /usr/bin/g++-9 --slave /usr/bin/gcov gcov /usr/bin/gcov-9
       sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 80 --slave /usr/bin/g++ g++ /usr/bin/g++-8 --slave /usr/bin/gcov gcov /usr/bin/gcov-8
       sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70 --slave /usr/bin/g++ g++ /usr/bin/g++-7 --slave /usr/bin/gcov gcov /usr/bin/gcov-7
  ```
  * (version 9 is the default one, then to change version use: ``sudo update-alternatives --config gcc``)
  * finally go into Clion settings -> Build, Execution, Deployment -> Toolchains and select compilers C e C++ as gcc-9 e g++-9 respectively (they should be in ``/usr/bin/gcc-9`` & ``/usr/bin/c++-9``)
* download wolfssl-4.5.0.zip from https://www.wolfssl.com/download/ and install it as in this guide https://www.wolfssl.com/docs/wolfssl-manual/ch2/ (2.2 Building on *nix)
```
cd path_to_wolfssl
./configure --enable-opensslextra --enable-tls13 --enable-maxstrength --enable-sni --enable-crl --enable-supportedcurves --enable-session-ticket --enable-harden
make
sudo make install
make test *(some tests may fail)*
```
* install protocol buffers as explained here: https://github.com/protocolbuffers/protobuf/blob/master/src/README.md#c-installation---windows*__* (C++ Installation - Unix)
* install sqlite3 as explained here: https://linuxhint.com/install_sqlite_browser_ubuntu_1804/ and also the dev version as here: https://zoomadmin.com/HowToInstall/UbuntuPackage/libsqlite3-dev

* (notice: as it is written above these steps are the ones to be executed for a ubuntu machine, if you have another OS commands may be different (probably similar though).. in any case the important thing is to install wolfssl, sqlite3, protocol buffers and have version 9 of gcc & g++)
## on windows:
* use Cygwin as configuration (it may already be the default one), install it if needed through https://cygwin.com/install.html --> if you need to install it then install also the protocol buffer library (see last point); then select it as compiler to use in CLion
* download wolfssl-4.5.0.zip from https://www.wolfssl.com/download/ and install it using Cygwin command prompt as in this guide https://www.wolfssl.com/docs/wolfssl-manual/ch2/ (2.2 Building on *nix)
```
cd path_to_wolfssl
./configure --enable-opensslextra --enable-tls13 --enable-maxstrength --enable-sni --enable-crl --enable-supportedcurves --enable-session-ticket --enable-harden
make
make install
make test *(some tests may fail)*
```
* install protocol buffer library using Cygwin installer (even if Cygwin has already been installed, we will proceed only to the installation of new packages) through https://cygwin.com/install.html; select the libraries ``libprotobuf23`` & ``libprotobuf-devel`` selecting for bot of them the version ``3.12.3-1`` and proceed to the installation; in the end add the installation path of cygwin to the environment path as seen in this guide https://docs.alfresco.com/4.2/tasks/fot-addpath.html -> add also a new system environment variable with the name = ``CYGWIN_ROOT`` (``C:\cygwin64`` in my case) as in this guide https://docs.oracle.com/en/database/oracle/r-enterprise/1.5.1/oread/creating-and-modifying-environment-variables-on-windows.html#GUID-DD6F9982-60D5-48F6-8270-A27EC53807D0.
* install SQLite3 using Cygwin installer (as already done with protocol buffers) (https://cygwin.com/install.html); select the libraries ``libsqlite3_0`` & ``libsqlite3-devel`` selecting for both the version ``3.32.3-1`` and proceed to the installation

## useful tools
* sequence diagrams maker: https://mermaid-js.github.io/mermaid-live-editor/ (how to use: https://mermaid-js.github.io/mermaid/)
* protocol buffers: https://github.com/protocolbuffers/protobuf/releases/tag/v3.13.0 (how to use: https://developers.google.com/protocol-buffers/docs/cpptutorial)
* sqlite browser: https://sqlitebrowser.org/dl/