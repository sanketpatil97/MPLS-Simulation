#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <cstring>
#define INF 999999
using namespace std;

// data structure to store link properties
struct link{
    int cost;
    double capacity;
};

// data structure to store path properties
struct path{
    int cost;
    string route;
};

// data structure to store connection request properties
struct conn_req{
    int id, source, dest, cost;
    double required_bw;
    bool isAdmitted;
    string labelPath;
};

// data structure to store forwarding table entries
struct forwardPath{
    int interface_in, label_in;
    int interface_out, label_out;
};

vector<vector<struct link>> topology;
vector<struct conn_req> connections;
vector<vector<struct path>> shortest_path1;
vector<vector<struct path>> shortest_path2;
vector<vector<struct forwardPath>> forwardingTable;
int n_nodes, n_edges, n_conn_req, admittedCount;
string topologyFile,routingTableFile, connectionsFile, forwardingTableFile, pathFile;
bool distMetric, approach;
int vcid = 100;

void printError(string errorMessage){
    cout << errorMessage << endl;
    exit(-1);
}

// initializing the vectors with appropriate sizes
void initializeVectors(){
    topology.assign(n_nodes, vector<struct link>(n_nodes,{INF,-1}));
    shortest_path1.assign(n_nodes, vector<struct path>(n_nodes));
    shortest_path2.assign(n_nodes, vector<struct path>(n_nodes));
    forwardingTable.assign(n_nodes, vector<struct forwardPath>());
}

// reading the topology file
void readTopology(string file_name, bool distMetric){
    ifstream ifs(file_name);
    if(!ifs.is_open()) printError("Could not open file " + file_name);

    ifs >> n_nodes >> n_edges;
    initializeVectors();

    for(int i=0; i<n_edges; ++i){
        int source, dest, cost;
        double capacity;
        ifs >> source >> dest >> cost >> capacity;

        topology[source][dest].capacity = capacity;
        topology[dest][source].capacity = capacity;

        // if cost is used as distance metric
        if(distMetric){
            topology[source][dest].cost = cost;
            topology[dest][source].cost = cost;
        }

        // if hop count is used as distance metric
        else{
            topology[source][dest].cost = 1;
            topology[dest][source].cost = 1;
        }
    }

    ifs.close();
}

// reading the connection file
void readConnections(string file_name, bool approach){
    ifstream ifs(file_name);
    if(!ifs.is_open()) printError("Could not open file " + file_name);

    ifs >> n_conn_req;
    connections.assign(n_conn_req, {-1, -1, -1, -1, 0.0, false, ""});
    for(int i=0; i< n_conn_req; ++i){
        int source, dest;
        double bw_min, bw_avg, bw_max;
        ifs >> source >> dest;
        ifs >> bw_min >> bw_avg >> bw_max;
        double bw_equi = min(bw_max, bw_avg + 0.25*(bw_max-bw_min));

        connections[i].id = i+1;
        connections[i].source = source;
        connections[i].dest = dest;
        if(approach) connections[i].required_bw = bw_equi;
        else connections[i].required_bw = bw_max;
    }

    ifs.close();
}

// applying Dijkstra's algorithm to find the shortest path
void applyDijkstras(vector<vector<struct link>> graph, int s, vector<int>& parent, vector<int>& cost){
    priority_queue<pair<int,int>, vector<pair<int,int>>,  greater<pair<int,int>>> pq; // creating min-heap
    pq.push(make_pair(0, s));
    parent[s] = s;
    cost[s] = 0;

    while(!pq.empty()){
        int u = pq.top().second;
        pq.pop();

        // changing distance for the adjacent nodes of u
        for(int v=0; v<n_nodes; ++v){
            if(cost[u] + graph[u][v].cost < cost[v]){
                cost[v] = cost[u] + graph[u][v].cost;
                parent[v] = u;
                pq.push(make_pair(cost[v], v));
            }
        }
    }
}

// tracing the shortest path from node u to node v
string tracePath(vector<int> parent, int u, int v){
    string route = "";
    if(parent[v] == -1) return route; 

    int curNode = v;
    route += to_string(v);

    // tracing route back from destination node to source node v
    while(curNode != u){
        int par = parent[curNode];
        route = to_string(par) + " " + route;
        curNode = par;
    }

    return route;
}

// removing first shortest path
void removeFirstShortestPath(vector<vector<struct link>>& graph, int u, int v){
    string route = shortest_path1[u][v].route;
    if(route == "") return;
     
    int idx = 0;
    int len = route.length();
    int curNode = u;
    while(idx<len && route[idx]!=' ') ++idx;

    // removing links in shortest path from node u to node v
    while(++idx < len){
        int nextNode = 0;
        while(idx<len && route[idx]!=' ') nextNode = nextNode*10 + (route[idx++]-'0');
        graph[curNode][nextNode].cost = INF;
        graph[nextNode][curNode].cost = INF;
        curNode = nextNode; 
    }
}

// storing routing tables for each node in file
void storeRoutingTables(string file_name){
    ofstream ofs(file_name);
    if(!ofs.is_open()) printError("Could not open file " + file_name);
    
    ofs << "Format : <Destination, Path Cost, Path>" << endl << endl;
    for(int u=0; u<n_nodes; ++u){
        // storing first routing table
        ofs << "First routing table for node " << u << ":" << endl;
        for(int v=0; v<n_nodes; ++v)
            ofs << v << "\t\t" << shortest_path1[u][v].cost << "\t\t" << shortest_path1[u][v].route << endl;
        ofs << "---------------------------------------------------------------------" << endl << endl;

        // storing second routing table
        ofs << "Second routing table for node " << u << ":" << endl;
        for(int v=0; v<n_nodes; ++v)
            ofs << v << "\t\t" << shortest_path2[u][v].cost << "\t\t" << shortest_path2[u][v].route << endl;
        ofs << "======================================================================" << endl << endl;
    }

    ofs.close();
}

// finding two shortest paths between every pair of nodes
void findShortestPaths(){
    // finding first shortest path between every pair of nodes
    for(int source=0; source<n_nodes; ++source){
        vector<int> parent(n_nodes, -1);
        vector<int> cost(n_nodes, INF);
        applyDijkstras(topology, source, parent, cost);

        for(int dest=0; dest<n_nodes; ++dest){
            shortest_path1[source][dest].route = tracePath(parent, source, dest);
            shortest_path1[source][dest].cost = cost[dest];
        }
    }

    // finding second shortest path between every pair of nodes
    for(int source=0; source<n_nodes; ++source){
        for(int dest=0; dest<n_nodes; ++dest){
            vector<vector<struct link>> residual_graph = topology;
            removeFirstShortestPath(residual_graph, source, dest);
            vector<int> parent(n_nodes, -1);
            vector<int> cost(n_nodes, INF);
            applyDijkstras(residual_graph, source, parent, cost);
            shortest_path2[source][dest].route = tracePath(parent, source, dest);
            shortest_path2[source][dest].cost = cost[dest];
        }
    }
}

// checking if the connection can be admitted based on link capacity
bool canAdmitConnection(string route, double required_bw){
    int len = route.length();
    if(len == 0) return false;

    int idx = 0;
    int curNode = 0;
    while(idx<len && route[idx]!=' ') curNode = curNode*10 + (route[idx++]-'0'); 

    while(++idx < len){
        int nextNode = 0;
        while(idx<len && route[idx]!=' ') nextNode = nextNode*10 + (route[idx++]-'0');
        if(topology[curNode][nextNode].capacity < required_bw) return false;
        curNode = nextNode;
    }

    // allocating bandwidth for the connection
    idx = 0;
    curNode = 0;
    while(idx<len && route[idx]!=' ') curNode = curNode*10 + (route[idx++]-'0');
    while(++idx < len){
        int nextNode = 0;
        while(idx<len && route[idx]!=' ') nextNode = nextNode*10 + (route[idx++]-'0');
        topology[curNode][nextNode].capacity -= required_bw;
        topology[nextNode][curNode].capacity -= required_bw;
        curNode = nextNode;
    }

    return true;
}

// adding entry in forwarding table for the admitted connections
string updateForwardingTable(string route){
    int idx = 0;
    int len = route.length();
    int inInterface = -1;
    int curNode = 0;
    int inLabel = -1;
    string labelPath = "";

    while(idx<len && route[idx]!=' ') curNode = curNode*10 + (route[idx++]-'0');
    while(++idx < len){
        int outInterface = 0;
        while(idx<len && route[idx]!=' ') outInterface = outInterface*10 + (route[idx++]-'0');
        int outLabel = ++vcid;
        struct forwardPath path = {inInterface, inLabel, outInterface, outLabel};
        forwardingTable[curNode].push_back(path);
        inInterface = curNode;
        curNode = outInterface;
        inLabel = outLabel;
        labelPath += to_string(inLabel) + " ";
    }
    forwardingTable[curNode].push_back({inInterface, inLabel, -1, -1});

    return labelPath;
}

// attempting to establish connections
void processConnectionRequests(){
    for(int i=0; i<n_conn_req; ++i){
        // checking if the connection can be established based on the link capacity
        string route1 = shortest_path1[connections[i].source][connections[i].dest].route;
        string route2 = shortest_path2[connections[i].source][connections[i].dest].route;

        // check if the connection can be admitted through first shortest path
        // add the corresponding forwarding table entries if the connection can be admitted
        if(canAdmitConnection(route1, connections[i].required_bw)){
            connections[i].isAdmitted = true;
            connections[i].labelPath = updateForwardingTable(route1);
            connections[i].cost = shortest_path1[connections[i].source][connections[i].dest].cost;
            admittedCount++;
        } 
            
        // check if the connection can be admitted through second shortest path
        // add the corresponding forwarding table entries if the connection can be admitted
        else if(canAdmitConnection(route2, connections[i].required_bw)){
            connections[i].isAdmitted = true;
            connections[i].labelPath = updateForwardingTable(route2);
            connections[i].cost = shortest_path2[connections[i].source][connections[i].dest].cost;
            admittedCount++;
        }
    }

    ofstream ofs(pathFile);
    if(!ofs.is_open()) printError("Could not open the file " + pathFile);
    ofs << "Number of connection requests:  " << n_conn_req << endl;
    ofs << "Number of admitted connection requests: " << admittedCount << endl;
    ofs.close();
}

// setting the parameters given using command line
void setParameters(int argc, char* argv[]){
    if(argc != 15) printError("Invalid arguments!");

    for(int i=1; i<15; i+=2){
        string option(argv[i]);
        string value(argv[i+1]);

        if(option == "-top") topologyFile = value;
        else if(option == "-conn") connectionsFile = value;
        else if(option == "-rt") routingTableFile = value;
        else if(option == "-ft") forwardingTableFile = value;
        else if(option == "-path") pathFile = value;
        
        else if(option == "-flag"){
            if(value == "dist") distMetric = true;
            else if(value == "hop") distMetric = false;
            else printError("Invalid value for option " + option);
        }

        else if(option == "-p"){
            if(value == "0") approach = true;
            else if(value == "1") approach = false;
            else printError("Invalid value for option " + option);
        }

        else printError("Invalid option " + option);
    }
}

// storing forward table in file
void storeForwardingTables(string file_name){
    ofstream ofs(file_name);
    if(!ofs.is_open()) printError("Could not open the file " + file_name);
    
    ofs << "Format : <Interface In, Label In, Interface Out, Label Out>" << endl << endl;
    for(int i=0; i<n_nodes; ++i){
        ofs << "Forwarding table for node " << i << ":" << endl;
        for(auto entry:forwardingTable[i]){
            if(entry.interface_in == -1) ofs << "--\t\t";
            else ofs << entry.interface_in << "\t\t";

            if(entry.label_in == -1) ofs << "--\t\t";
            else ofs << entry.label_in << "\t\t";

            if(entry.interface_out == -1) ofs << "--\t\t";
            else ofs << entry.interface_out << "\t\t";

            if(entry.label_out == -1) ofs << "--" << endl;
            else ofs << entry.label_out << endl;
        }
        ofs << "------------------------------------------" << endl << endl;
    }
    ofs.close();
}

// storing established connection details in file
void storeLabelPaths(string file_name){
    ofstream ofs(file_name);
    if(!ofs.is_open()) printError("Colud not open file " + file_name);

    ofs << "Format : <Connection ID, Source, Destination, Path Cost, Label List>" << endl << endl;
    for(auto request:connections){
        if(request.isAdmitted){
            ofs << request.id << "\t\t" << request.source << "\t\t" << request.dest << "\t\t" << request.cost << "\t\t" << request.labelPath << endl;
        }
    }

    ofs.close();
}

int main(int argc, char* argv[]){
    // configuring parameters
    setParameters(argc, argv);

    // creating routing table
    readTopology(topologyFile, distMetric);
    findShortestPaths();
    storeRoutingTables(routingTableFile);

    // creating forwarding table
    readConnections(connectionsFile, approach);
    processConnectionRequests();
    storeForwardingTables(forwardingTableFile);
    storeLabelPaths("Output.txt");
    
    return 0;
}