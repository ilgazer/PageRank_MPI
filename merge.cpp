#include <cstdio>
#include <iostream>
#include <ostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mpi.h>
#include <sstream>
#include <array>
#include "utils.h"

typedef long node_id;
typedef double rank;
typedef std::pair<node_id, rank> node_pair;

class Node
{
public:
  node_id id;
  int num_outgoing_edges;
  std::vector<node_id> incoming_edges;
};

static std::unordered_map<int, int> site_thread_mapping;

static const double alpha = 0.2;
static std::unordered_map<int, Node *> owned_nodes_by_id;

static std::vector<Node *> owned_nodes;
// For every pid, stores the list of local nodes that thread is interested in.
static std::vector<std::vector<node_id>> out_facing_nodes_by_pid;
// Number of nodes we expect to receive from this pid
static std::vector<size_t> num_incoming_nodes_by_pid;

static std::unordered_map<node_id, rank> *curr_ranks;
static std::unordered_map<node_id, rank> *next_ranks;
static std::unordered_map<node_id, rank> *scores;

static int num_iterations = 1;

int mypid;
int numprocs;

Node *get_node_by_id(const int &id)
{
  if (owned_nodes_by_id.find(id) == owned_nodes_by_id.end())
  {
    owned_nodes_by_id.insert({id, new Node{id, 0}});
    owned_nodes.push_back(owned_nodes_by_id[id]);
  }
  return owned_nodes_by_id[id];
}

void send_ranks()
{
  for (int dest_pid = 0; dest_pid < numprocs; dest_pid++)
  {
    auto &&nodes = out_facing_nodes_by_pid[dest_pid];

    if (nodes.size() == 0)
    {
      continue;
    }

    node_pair rank_updates[nodes.size()];
    for (size_t i = 0; i < nodes.size(); i++)
    {
      rank_updates[i].first = nodes[i];
      rank_updates[i].second = (*next_ranks)[nodes[i]];

      // std::cout << mypid << "-> rank_update_to" << dest_pid << " " << rank_updates[i] << "\n";

    }


    MPI_Ssend(rank_updates, nodes.size() * sizeof(node_pair), MPI_CHAR, dest_pid, 1, MPI_COMM_WORLD);
    // std::cout << "Sent ranks to " << dest_pid << " as pid:" << mypid << "\n";
  }
}

void recv_ranks_for(int pid)
{
  size_t num_incoming_nodes = num_incoming_nodes_by_pid[pid];
  if (num_incoming_nodes == 0)
    return;

  auto rank_updates = new node_pair[num_incoming_nodes];
  MPI_Status stat;
  MPI_Recv(rank_updates, num_incoming_nodes * sizeof(node_pair), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);
  // std::cout << "Received ranks from " << pid << " as pid:" << mypid << "\n";
  (*next_ranks).insert(rank_updates, rank_updates + num_incoming_nodes);
}

void sync_ranks()
{
  for (int curr_pid = 0; curr_pid < numprocs; curr_pid++)
  {
    if (curr_pid == mypid)
    {
      send_ranks();
    }
    else
    {
      recv_ranks_for(curr_pid);
    }
  }
  std::swap(curr_ranks, next_ranks);
  next_ranks->clear();
}

double calculate_ranks()
{
  double sigma = 0;
  for (Node *node : owned_nodes)
  {
    double sum = 0;
    for (node_id inc_edge : node->incoming_edges)
    {
      sum += (*curr_ranks)[inc_edge];
    }
    rank rank = sum * alpha + (1 - alpha);

    sigma += std::abs((*scores)[node->id] - rank);
    (*scores)[node->id] = rank;

    if (node->num_outgoing_edges)
      (*next_ranks)[node->id] = rank / (node->num_outgoing_edges);

    // std::cout << node->id << ":" << rank << "," << node->num_outgoing_edges << "\n";
  }
  return sigma;
}

int communicate_sigma(double sigma)
{
  if (mypid != 0)
  {
    MPI_Ssend(&sigma, sizeof(double), MPI_CHAR, 0, 1, MPI_COMM_WORLD);
    // std::cout << "Sent sigma " << sigma << " as pid:" << mypid << "\n";
    int cont;
    MPI_Status stat;
    MPI_Recv(&cont, sizeof(int), MPI_CHAR, 0, 1, MPI_COMM_WORLD, &stat);
    // std::cout << "Received goahead? " << cont << " as pid:" << mypid << "\n";
    return cont;
  }
  else
  {
    double sum_sigmas = sigma;
    // std::cout << "My sigma " << sigma << "\n";

    for (int pid = 1; pid < numprocs; pid++)
    {
      double recv_sigma;
      MPI_Status stat;
      MPI_Recv(&recv_sigma, sizeof(double), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);
      // std::cout << "Received sigma " << recv_sigma << " from pid:" << pid << "\n";
      sum_sigmas += recv_sigma;
    }
    // std::cout << "Sigma" << num_iterations << "=" << sum_sigmas << "\n";
    int cont = sum_sigmas > 1e-6;
    for (int pid = 1; pid < numprocs; pid++)
    {
      MPI_Ssend(&cont, sizeof(int), MPI_CHAR, pid, 1, MPI_COMM_WORLD);
      // std::cout << "Sent goahead? " << cont << " to pid:" initi<< pid << "\n";
    }
    return cont;
  }
}

template <typename T, typename K>
std::array<node_pair, 5> find_top_five(T *t, K get_pair)
{
  std::array<node_pair, 5> maximums;
  std::fill(maximums.begin(), maximums.end(), node_pair(-1, -1));

  for (auto &&raw : *t)
  {
    node_pair item = get_pair(raw);
    size_t breaking = -1;
    for (size_t j = 0; j < 5; j++)
    {
      if (maximums[j].second <= item.second)
      {
        breaking = j;
      }
      else
      {
        break;
      }
    }
    if (breaking != -1)
    {
      for (size_t j = 0; j < breaking; j++)
      {
        maximums[j] = maximums[j + 1];
      }
      maximums[breaking] = item;
    }
  }
  return maximums;
}

void consolidate_top_five()
{
  auto local_top_five = find_top_five(
      &owned_nodes,
      [](Node * node) -> auto{ return node_pair(node->id, (*scores)[node->id]); });

  if (mypid != 0)
  {
    MPI_Ssend(&local_top_five, sizeof(local_top_five), MPI_CHAR, 0, 1, MPI_COMM_WORLD);
    // std::cout << "Sent top 5 as pid " << mypid << "\n";
  }
  else
  {
    std::vector<node_pair> candidates(5 * numprocs);
    std::copy(local_top_five.begin(), local_top_five.end(), candidates.begin());
    for (int pid = 1; pid < numprocs; pid++)
    {
      MPI_Status stat;
      MPI_Recv(&candidates[5 * pid], sizeof(local_top_five), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);
      // std::cout << "Received top 5 from pid " << pid << "\n";
    }
    auto global_top_five = find_top_five(
        &candidates, [](auto candidate) -> auto{ return candidate; });
    std::cout << global_top_five << "\n";
  }
}

void init_vars()
{
  num_incoming_nodes_by_pid.reserve(numprocs);
  out_facing_nodes_by_pid.reserve(numprocs);

  for (int pid = 0; pid < numprocs; pid++)
  {
    num_incoming_nodes_by_pid.push_back(0);
    out_facing_nodes_by_pid.push_back(std::vector<node_id>());
  }
}

void push_mock_data()
{
  if (mypid == 0)
  {
    owned_nodes.push_back(new Node{0, 2, {1}});

    num_incoming_nodes_by_pid[1] = 3;
    out_facing_nodes_by_pid[1].push_back(0);
  }
  else if (mypid == 1)
  {
    owned_nodes.push_back(new Node{1, 1, {0, 2, 3}});
    owned_nodes.push_back(new Node{2, 1, {0}});

    num_incoming_nodes_by_pid[0] = 1;
    num_incoming_nodes_by_pid[2] = 1;
    out_facing_nodes_by_pid[0].push_back(1);
    out_facing_nodes_by_pid[0].push_back(2);
  }
  else if (mypid == 2)
  {
    owned_nodes.push_back(new Node{3, 1, {}});

    out_facing_nodes_by_pid[1].push_back(3);
  }
}

void init_ranks()
{
  // 1850065
  for (auto &&node : owned_nodes)
  {
    (*scores)[node->id] = 1.0;
    if (node->num_outgoing_edges)
    {
      (*next_ranks)[node->id] = 1.0 / node->num_outgoing_edges;
      (*curr_ranks)[node->id] = 1.0 / node->num_outgoing_edges;
    }
  }
}

int main(int argc, char *argv[])
{
  int src;
  int dest;
  int *sdata;
  int *rdata;
  int count;
  MPI_Status stat;
  MPI_Request req;

  int rc;

  MPI_Init(&argc, &argv); /* starts MPI */

  MPI_Comm_rank(MPI_COMM_WORLD, &mypid);    /* get current process id */
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs); /* get number of processes */

  MPI_Status stat_buff[2 * numprocs + 1];
  MPI_Request req_buff[2 * numprocs + 1];
  std::vector<int> senders;
  std::vector<int> receivers;

  if (mypid == 0)
  {

    std::ifstream edges(argv[1]);
    std::ifstream metis_threads(argv[2]);

    std::vector<std::vector<int>> thread_edges_sender;
    std::vector<std::vector<int>> thread_edges_receiver;
    std::vector<std::unordered_map<int, int>> thread_node_mapping;
    thread_edges_sender.resize(numprocs);
    thread_edges_receiver.resize(numprocs);
    thread_node_mapping.resize(numprocs);

    std::string sender, receiver;
    std::string thread;
    static std::unordered_map<int, int> node_thread_mapping;
    int counter = 1;
    while (metis_threads >> thread)
    {
      int thread_id = stoi(thread);
      int node_id = counter;
      counter++;
      node_thread_mapping.insert({node_id, thread_id});
    }
    while (edges >> sender)
    {
      edges >> receiver;
      int sender_id = stoi(sender);
      int receiver_id = stoi(receiver);

      int sender_thread = node_thread_mapping[sender_id];
      int receiver_thread = node_thread_mapping[receiver_id];

      thread_edges_sender[sender_thread].push_back(sender_id);
      thread_edges_receiver[sender_thread].push_back(receiver_id);

      // thread_node_mapping[sender_thread][sender_id] = sender_thread;
      if (sender_thread != receiver_thread)
      {
        thread_edges_sender[receiver_thread].push_back(sender_id);
        thread_edges_receiver[receiver_thread].push_back(receiver_id);

        thread_node_mapping[sender_thread][receiver_id] = receiver_thread;
        thread_node_mapping[receiver_thread][sender_id] = sender_thread;
        // thread_node_mapping[receiver_thread][receiver_id] = receiver_thread;
      }
    }

    // sending senders
    for (int i = 1; i < numprocs; i++)
    {
      sdata = (int *)calloc(1, sizeof(int));
      sdata[0] = thread_edges_sender[i].size();
      // std::cout << "Senders are : "<<thread_edges_sender[i] <<" to thread " << i << std::endl;
      count = 1;
      MPI_Send(sdata, count, MPI_INT, i, 0, MPI_COMM_WORLD);
      // rc = MPI_Waitall(2,req_buff,stat_buff) ;
      count = thread_edges_sender[i].size();
      int *nodes = thread_edges_sender[i].data();
      // std::cout << "nodes last element : "<< nodes[count-1] << std::endl;
      MPI_Isend(nodes, count, MPI_INT, i, i * 10 + 1, MPI_COMM_WORLD, &req_buff[i]);
    }
    senders = thread_edges_sender[0];
    for (int i = 1; i < numprocs; i++)
    {
      MPI_Wait(&req_buff[i], MPI_STATUS_IGNORE);
    }
    // sending receivers
    for (int i = 1; i < numprocs; i++)
    {
      sdata = (int *)calloc(1, sizeof(int));
      sdata[0] = thread_edges_sender[i].size();
      // std::cout << "Receivers are : "<<thread_edges_receiver[i] <<" to thread " << i << std::endl;
      // rc = MPI_Waitall(2,req_buff,stat_buff) ;
      count = thread_edges_receiver[i].size();
      int *nodes = thread_edges_receiver[i].data();
      // std::cout << "nodes last element : "<< nodes[count-1] << std::endl;
      MPI_Isend(nodes, count, MPI_INT, i, i * 10 + 1, MPI_COMM_WORLD, &req_buff[i]);
    }
    receivers = thread_edges_receiver[0];
    for (int i = 1; i < numprocs; i++)
    {
      MPI_Wait(&req_buff[i], MPI_STATUS_IGNORE);
    }
    // sending node thread mapping
    
    for (int i = 1; i < numprocs; i++)
    {
      std::vector<int> node_thread_mapping_keys;
      std::vector<int> node_thread_mapping_values;
      for (const auto &node_thread_pair : thread_node_mapping[i])
      {
        node_thread_mapping_keys.push_back(node_thread_pair.first);
        node_thread_mapping_values.push_back(node_thread_pair.second);
        // std::cout << node_thread_pair.first << " " << node_thread_pair.second<< "\n";
      }
      for (int k = 0; k < node_thread_mapping_keys.size(); k++)
        // std::cout << node_thread_mapping_keys[k] << " " << node_thread_mapping_values[k]<< "\n";
        sdata = (int *)calloc(1, sizeof(int));
      sdata[0] = node_thread_mapping_keys.size();
      // std::cout << "Keys are : "<<node_thread_mapping_keys <<" to thread " << i << std::endl;
      // std::cout << "values are : "<<node_thread_mapping_values <<" to thread " << i << std::endl;
      count = 1;
      MPI_Send(sdata, count, MPI_INT, i, 0, MPI_COMM_WORLD);
      count = node_thread_mapping_keys.size();
      // std::cout << "nodes last element : "<< nodes[count-1] << std::endl;
      int *keys = node_thread_mapping_keys.data();
      MPI_Isend(keys, count, MPI_INT, i, i * 10 + 1, MPI_COMM_WORLD, &req_buff[i]);
      // for(int i = 1; i < numprocs; i ++){
      MPI_Wait(&req_buff[i], MPI_STATUS_IGNORE);
      // MPI_Wait(&req_buff[numprocs +  i -1], MPI_STATUS_IGNORE);
      // std::cout << "[0] : Transmission ended to thread " << i << std::endl;
      // }

      int *nodes = node_thread_mapping_values.data();
      MPI_Isend(nodes, count, MPI_INT, i, i * 10 + 2, MPI_COMM_WORLD, &req_buff[numprocs + i - 1]);
      MPI_Wait(&req_buff[numprocs + i - 1], MPI_STATUS_IGNORE);
    }
    site_thread_mapping = thread_node_mapping[0];
    // std::cout << mypid << "done with receiving\n";
  }
  else
  {
    rdata = (int *)calloc(1, sizeof(int));
    src = 0;
    MPI_Recv(rdata, 1, MPI_INT, src, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
    // rc = MPI_Waitall(2,req_buff,stat_buff) ;
    senders.resize(rdata[0]);
    MPI_Irecv(&senders[0], rdata[0], MPI_INT, src, MPI_ANY_TAG, MPI_COMM_WORLD, &req_buff[0]);
    MPI_Wait(&req_buff[0], MPI_STATUS_IGNORE);
    // printf("[%d] received it from %d with tag %d: rdata 0 is %d , last element is  %d\n",mypid,stat.MPI_SOURCE,stat.MPI_TAG, rdata[0], senders[rdata[0] -1]);
    receivers.resize(rdata[0]);
    MPI_Irecv(&receivers[0], rdata[0], MPI_INT, src, MPI_ANY_TAG, MPI_COMM_WORLD, &req_buff[0]);
    MPI_Wait(&req_buff[0], MPI_STATUS_IGNORE);
    // printf("[%d] received it from %d with tag %d: rdata 0 is %d , last element is  %d\n",mypid,stat.MPI_SOURCE,stat.MPI_TAG, rdata[0], senders[rdata[0] -1]);
    MPI_Recv(rdata, 1, MPI_INT, src, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
    std::vector<int> mapping_keys;
    std::vector<int> mapping_values;
    mapping_keys.resize(rdata[0]);
    mapping_values.resize(rdata[0]);
    MPI_Irecv(&mapping_keys[0], rdata[0], MPI_INT, src, mypid * 10 + 1, MPI_COMM_WORLD, &req_buff[0]);
    MPI_Wait(&req_buff[0], MPI_STATUS_IGNORE);
    MPI_Irecv(&mapping_values[0], rdata[0], MPI_INT, src, mypid * 10 + 2, MPI_COMM_WORLD, &req_buff[1]);
    MPI_Wait(&req_buff[1], MPI_STATUS_IGNORE);
    for (int i = 0; i < rdata[0]; i++)
    {
      site_thread_mapping[mapping_keys[i]] = mapping_values[i];
    }
    // std::cout << mypid << "done with receiving\n";
  }
  init_vars();
  int i;
  for (i = 0; i < senders.size(); i++)
  {
    // sender is this processes node
    if (site_thread_mapping.find(senders[i]) == site_thread_mapping.end())
    {
      Node *sender = get_node_by_id(senders[i]);
      sender->num_outgoing_edges++;
    }
    else
    {
      num_incoming_nodes_by_pid[site_thread_mapping[senders[i]]]++;
    }

    // receiver is this processes node
    if (site_thread_mapping.find(receivers[i]) == site_thread_mapping.end())
    {
      Node *receiver = get_node_by_id(receivers[i]);
      receiver->incoming_edges.push_back(senders[i]);
    }
    else
    {
      out_facing_nodes_by_pid[site_thread_mapping[receivers[i]]].push_back(senders[i]);
    }
  }
  // std::cout << mypid << " " << owned_nodes.size() << std::endl;
  // std::cout << mypid << out_facing_nodes_by_pid << "\n";
  curr_ranks = new std::unordered_map<node_id, rank>;
  next_ranks = new std::unordered_map<node_id, rank>;
  scores = new std::unordered_map<node_id, rank>;
  init_ranks();
  int cont = 1;
  std::cout << "done init at " << mypid << std::endl;
  int asd = 0;
  while (cont)
  {
    // std::cout << mypid << "-> next_ranks" << *next_ranks << "\n";
    sync_ranks();
    std::cout << "done round " << asd++ <<" at " << mypid << std::endl;
    // std::cout << mypid << "-> curr_ranks" << *curr_ranks << "\n";
    double sigma = calculate_ranks();
    // std::cout << mypid << "-> next_ranks" << *next_ranks << "\n";
    // std::cout << mypid << "-> scores" << *scores << "\n";
    cont = communicate_sigma(sigma);
    num_iterations++;
    // std::cout << std::endl;
  }
    std::cout << "done calculating at " << mypid << std::endl;

  // std::cout << "Exited loop as pid " << mypid << "\n";
  consolidate_top_five();
  // std::cout << num_iterations << " iterations\n";

  MPI_Finalize();
  return 0;
}
