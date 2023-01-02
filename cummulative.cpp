#include <cstdio>
#include <iostream>
#include <ostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mpi.h>
#include <sstream>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;

long timed_portion(int row_begin_size, int* row_begin, int* col_indices, double* values, int& sigma_count);
void do_row(int row_begin_size, int* row_begin, int* col_indices, double* values, std::ostream &out);
void print_csr(int row_begin_size, int i, int* row_begin, int* col_indices, double* values);

template <typename S> std::ostream& operator<<(std::ostream& os,
                    const std::vector<S>& vector)
{
    // Printing all the elements
    // using <<
    os << "[";
    for (S element : vector) {
        os << element << ", ";
    }
    os << "]";
    return os;
}
void print_csr(int row_begin_size, int i, int* row_begin, int* col_indices, double* values);
class Site
{
public:
    int name;
    int id;
    int num_outgoing_edges;
    std::vector<int> incoming_edges;

    friend std::ostream& operator<<(std::ostream& os, Site const & s) {
        return os << "Site{name="<<s.name<<" id="<<s.id<<", incoming_edges=" << s.incoming_edges << ", num_outgoing_edges="<<s.num_outgoing_edges << "}";
    }
};

static std::unordered_map<int, Site *> sites_by_ref;
static std::unordered_map<int, int> site_thread_mapping;
static std::vector<Site *> sites_by_id;

Site* get_site(const int  &name)
{
    Site *site;
    if (sites_by_ref.count(name) == 0)
    {
        site = new Site{name, (int)sites_by_id.size(), 0};
        sites_by_id.push_back(site);
        sites_by_ref.insert({name, site});
    }
    else
    {
        site = sites_by_ref[name];
    }
    return site;
}


int main(int argc, char *argv[])
{
  int   mypid     ;
  int   numprocs  ;
  int   src       ;
  int   dest      ;
  int  *sdata     ; 
  int  *rdata     ; 
  int   count     ; 
  MPI_Status stat ; 
  MPI_Request req ; 

  int rc ; 

  MPI_Init (&argc, &argv);                        /* starts MPI */

  MPI_Comm_rank (MPI_COMM_WORLD, &mypid);        /* get current process id */
  MPI_Comm_size (MPI_COMM_WORLD, &numprocs);     /* get number of processes */

  MPI_Status stat_buff[2 * numprocs + 1] ; 
  MPI_Request req_buff[2 * numprocs+1] ;
  std::vector<int> senders;
  std::vector<int> receivers;
  
  if(mypid == 0){
    std::cout << mypid <<": reading data" << std::endl;
    
    std::ifstream edges(argv[1]);
    std::ifstream metis_threads(argv[2]);
    
    std::vector<std::vector<int> >thread_edges_sender;
    std::vector<std::vector<int> >thread_edges_receiver;
    std::vector<std::unordered_map<int,int>>thread_node_mapping;
    thread_edges_sender.resize(numprocs);
    thread_edges_receiver.resize(numprocs);
    thread_node_mapping.resize(numprocs);
    
    std::string sender, receiver;
    std::string thread;
    static std::unordered_map<int, int> node_thread_mapping;
    int counter = 1;
    while (metis_threads >> thread){
        
        int thread_id = stoi(thread) ;
        int node_id = counter;
        counter ++;
        node_thread_mapping.insert({node_id, thread_id});
    }
    while (edges >> sender){
        edges >> receiver;
        int sender_id = stoi(sender) ;
        int receiver_id = stoi(receiver) ;
        
        int sender_thread = node_thread_mapping[sender_id];
        int receiver_thread = node_thread_mapping[receiver_id];

        thread_edges_sender[sender_thread].push_back(sender_id);
        thread_edges_receiver[sender_thread].push_back(receiver_id);

        // thread_node_mapping[sender_thread][sender_id] = sender_thread;
        if(sender_thread != receiver_thread){
            thread_edges_sender[receiver_thread].push_back(sender_id);
            thread_edges_receiver[receiver_thread].push_back(receiver_id);

            thread_node_mapping[sender_thread][receiver_id] = receiver_thread;
            thread_node_mapping[receiver_thread][sender_id] = sender_thread;
            // thread_node_mapping[receiver_thread][receiver_id] = receiver_thread;
        }
    }

    //sending senders
    for(int i = 1; i < numprocs; i ++){
        sdata = (int *) calloc(1,sizeof(int))  ;
        sdata[0] = thread_edges_sender[i].size();
        // std::cout << "Senders are : "<<thread_edges_sender[i] <<" to thread " << i << std::endl;
        count = 1;
        MPI_Send(sdata,count,MPI_INT,i,0,MPI_COMM_WORLD); 
        // rc = MPI_Waitall(2,req_buff,stat_buff) ;
        count = thread_edges_sender[i].size() ;
        int * nodes = thread_edges_sender[i].data();
        // std::cout << "nodes last element : "<< nodes[count-1] << std::endl;
        MPI_Isend(nodes,count,MPI_INT,i,i*10 + 1,MPI_COMM_WORLD,&req_buff[i]) ; 
    }
    senders = thread_edges_sender[0];
    for(int i = 1; i < numprocs; i ++){
    MPI_Wait(&req_buff[i], MPI_STATUS_IGNORE);
    }
    //sending receivers
    for(int i = 1; i < numprocs; i ++){
        sdata = (int *) calloc(1,sizeof(int))  ;
        sdata[0] = thread_edges_sender[i].size();
        // std::cout << "Receivers are : "<<thread_edges_receiver[i] <<" to thread " << i << std::endl;
        // rc = MPI_Waitall(2,req_buff,stat_buff) ;
        count = thread_edges_receiver[i].size() ;
        int * nodes = thread_edges_receiver[i].data();
        // std::cout << "nodes last element : "<< nodes[count-1] << std::endl;
        MPI_Isend(nodes,count,MPI_INT,i,i*10 + 1,MPI_COMM_WORLD,&req_buff[i]) ; 
    }
    receivers = thread_edges_receiver[0];
    for(int i = 1; i < numprocs; i ++){
    MPI_Wait(&req_buff[i], MPI_STATUS_IGNORE);
    }
    //sending node thread mapping
    for(int i = 1; i < numprocs; i ++){
        std::vector<int> node_thread_mapping_keys;
        std::vector<int> node_thread_mapping_values;
        for(const auto &node_thread_pair: thread_node_mapping[i]){
            node_thread_mapping_keys.push_back(node_thread_pair.first);
            node_thread_mapping_values.push_back(node_thread_pair.second);
            // std::cout << node_thread_pair.first << " " << node_thread_pair.second<< "\n";
        }
        for(int k = 0; k<node_thread_mapping_keys.size(); k++)
        // std::cout << node_thread_mapping_keys[k] << " " << node_thread_mapping_values[k]<< "\n";
        sdata = (int *) calloc(1,sizeof(int))  ;
        sdata[0] = node_thread_mapping_keys.size();
        // std::cout << "Keys are : "<<node_thread_mapping_keys <<" to thread " << i << std::endl;
        // std::cout << "values are : "<<node_thread_mapping_values <<" to thread " << i << std::endl;
        count = 1;
        MPI_Send(sdata,count,MPI_INT,i,0,MPI_COMM_WORLD); 
        count = node_thread_mapping_keys.size() ;
        // std::cout << "nodes last element : "<< nodes[count-1] << std::endl;
        int* keys = node_thread_mapping_keys.data();
        MPI_Isend(keys,count,MPI_INT,i,i*10 + 1,MPI_COMM_WORLD,&req_buff[i]) ; 
        // for(int i = 1; i < numprocs; i ++){
        MPI_Wait(&req_buff[i], MPI_STATUS_IGNORE);
        // MPI_Wait(&req_buff[numprocs +  i -1], MPI_STATUS_IGNORE);
    // std::cout << "[0] : Transmission ended to thread " << i << std::endl;
    // }

        int * nodes = node_thread_mapping_values.data();
        MPI_Isend(nodes,count,MPI_INT,i,i*10 + 2,MPI_COMM_WORLD,&req_buff[numprocs +  i -1]) ;
        MPI_Wait(&req_buff[numprocs +  i -1], MPI_STATUS_IGNORE);
    }
    site_thread_mapping = thread_node_mapping[0];
    
  }
  else{
    rdata = (int *) calloc(1,sizeof(int)) ;
    src = 0;
    MPI_Recv(rdata,1,MPI_INT,src,MPI_ANY_TAG,MPI_COMM_WORLD,&stat) ;
    // rc = MPI_Waitall(2,req_buff,stat_buff) ;  
    senders.resize(rdata[0]);
    MPI_Irecv(&senders[0],rdata[0],MPI_INT,src,MPI_ANY_TAG,MPI_COMM_WORLD,&req_buff[0]) ; 
    MPI_Wait(&req_buff[0],MPI_STATUS_IGNORE) ; 
    //printf("[%d] received it from %d with tag %d: rdata 0 is %d , last element is  %d\n",mypid,stat.MPI_SOURCE,stat.MPI_TAG, rdata[0], senders[rdata[0] -1]);
    receivers.resize(rdata[0]);
    MPI_Irecv(&receivers[0],rdata[0],MPI_INT,src,MPI_ANY_TAG,MPI_COMM_WORLD,&req_buff[0]) ; 
    MPI_Wait(&req_buff[0],MPI_STATUS_IGNORE) ; 
    //printf("[%d] received it from %d with tag %d: rdata 0 is %d , last element is  %d\n",mypid,stat.MPI_SOURCE,stat.MPI_TAG, rdata[0], senders[rdata[0] -1]);
    MPI_Recv(rdata,1,MPI_INT,src,MPI_ANY_TAG,MPI_COMM_WORLD,&stat) ;
    std::vector<int> mapping_keys;
    std::vector<int> mapping_values;
    mapping_keys.resize(rdata[0]);
    mapping_values.resize(rdata[0]);
    MPI_Irecv(&mapping_keys[0],rdata[0],MPI_INT,src,mypid*10 + 1,MPI_COMM_WORLD,&req_buff[0]) ;
    MPI_Wait(&req_buff[0],MPI_STATUS_IGNORE);
    MPI_Irecv(&mapping_values[0],rdata[0],MPI_INT,src,mypid*10 + 2,MPI_COMM_WORLD,&req_buff[1]) ;
    MPI_Wait(&req_buff[1],MPI_STATUS_IGNORE);
    for(int i = 0; i < rdata[0]; i ++){
        site_thread_mapping[mapping_keys[i]] = mapping_values[i];
    }
}
    // std::stringstream senders_str;
    // std::stringstream receivers_str;
    // std::stringstream thread_mapping_keys;
    // std::stringstream thread_mapping_values;
    // senders_str << senders;
    // receivers_str<< receivers;
    // int i = 0;
    // // for (auto const &pair: site_thread_mapping) {
    // //     if(i < 50){

    // //     thread_mapping_keys  << pair.first << " ";
    // //     thread_mapping_values  << pair.second << " ";
    // //     }
    // //     i ++ ;
    // //     if(site_thread_mapping[pair.first] != pair.second)
    // //     printf("%d: mismatch on %d, file: %d, map: %d", mypid,pair.first ,site_thread_mapping[pair.first], pair.second );
    // // }
    // printf("[%d]: senders :  \n receivers: \n keys: %s\n valu: %s\n size: %d\n",
        // mypid,
        // thread_mapping_keys.str().c_str(),
        // thread_mapping_values.str().c_str(),
        // site_thread_mapping.size()
        // );
    for(int i = 0; i < senders.size(); i ++)
    {
        int first_token = senders[i];
        int second_token = receivers[i];
        
        Site* from = get_site(first_token);
        Site* to = get_site(second_token);
        from->num_outgoing_edges++;
        to->incoming_edges.push_back(from->id);
    }

    std::cout << mypid <<": done with creating sites" << std::endl;
    /*for (Site* element : sites_by_id) {
        std::cout << *element  << std::endl;
    }*/

    size_t row_begin_size = sites_by_id.size() + 1;
    int* row_begin = new int[row_begin_size];

    int next_row_begin = 0;
    row_begin[0] = 0;
    int row_size;
    for(int i = 1; i < row_begin_size; i++){
        row_size = sites_by_id[i - 1]->incoming_edges.size();
        next_row_begin += row_size;
        row_begin[i] = next_row_begin;
    }

    double* values = new double[next_row_begin];
    int* col_indices = new int[next_row_begin];

    for(int i = 0; i < row_begin_size - 1; i++){
        int this_row_begin = row_begin[i];
        Site* site = sites_by_id[i];
        for (size_t j = 0; j < site->incoming_edges.size(); j++)
        {
            int to = site->incoming_edges[j];
            col_indices[this_row_begin+j] = to;
            values[this_row_begin+j] = 1.0/sites_by_id[to]->num_outgoing_edges;
        }
    }
    printf("[%d]: num_sites: %d\n",
        mypid,
        sites_by_id.size()
        );




  MPI_Finalize();
  return 0;
}
void print_csr(int row_begin_size, int next_row_begin, int* row_begin, int* col_indices, double* values)
{
    std::ofstream rb("csr.txt");

    for (size_t i = 0; i < row_begin_size; i++)
    {
        rb << row_begin[i] << " ";
    }
    rb << "\n";
    for (size_t i = 0; i < next_row_begin; i++)
    {
        rb << values[i] << " ";
    }
    rb << "\n";
    for (size_t i = 0; i < next_row_begin; i++)
    {
        rb << col_indices[i] << " ";
    }
    rb << "\n";
}