#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <array>

typedef long node_id;
typedef double rank;
typedef std::pair<node_id, rank> node_pair;

template <typename S>
std::ostream &operator<<(std::ostream &os,
                         const std::vector<S> &vector)
{
  // Printing all the elements
  // using <<
  os << "[";
  for (S element : vector)
  {
    os << element << ", ";
  }
  os << "]";
  return os;
}

template <typename S, size_t N>
std::ostream &operator<<(std::ostream &os,
                         const std::array<S, N> &vector)
{
  // Printing all the elements
  // using <<
  os << "[";
  for (S element : vector)
  {
    os << element << ", ";
  }
  os << "]";
  return os;
}

class Node
{
public:
  node_id id;
  int num_outgoing_edges;
  std::vector<node_id> incoming_edges;
};

static const double alpha = 0.8;

static std::vector<Node *> owned_nodes;

static std::unordered_map<node_id, rank> *curr_ranks;
static std::unordered_map<node_id, rank> *next_ranks;
// For every pid, stores the list of local nodes that thread is interested in.
static std::vector<std::vector<node_id>> out_facing_nodes_by_pid;
// Number of nodes we expect to receive from this pid
static std::vector<size_t> num_incoming_nodes_by_pid;

int mypid;
int numprocs;

void send_ranks()
{
  for (int dest_pid = 0; dest_pid < numprocs; dest_pid++)
  {
    auto &&nodes = out_facing_nodes_by_pid[dest_pid];

    if (nodes.size() == 0)
    {
      continue;
    }

    auto rank_updates = new node_pair[nodes.size()];
    for (size_t i = 0; i < nodes.size(); i++)
    {
      rank_updates[i].first = nodes[i];
      rank_updates[i].second = (*next_ranks)[nodes[i]];
    }

    MPI_Ssend(rank_updates, nodes.size() * sizeof(node_pair), MPI_CHAR, dest_pid, 1, MPI_COMM_WORLD);
  }
}

void recv_ranks_for(int pid)
{
  size_t num_incoming_nodes = num_incoming_nodes_by_pid[pid];
  auto rank_updates = new node_pair[num_incoming_nodes];
  MPI_Status stat;
  MPI_Recv(rank_updates, num_incoming_nodes * sizeof(node_pair), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);

  (*next_ranks).insert(rank_updates, rank_updates + num_incoming_nodes);
}

void sync_ranks()
{
  for (int curr_pid = 0; curr_pid < numprocs; curr_pid++)
  {
    if (curr_pid == mypid)
    {
      std::cout << "sending thread " << mypid << "\n";
      send_ranks();
    }
    else
    {
      recv_ranks_for(curr_pid);
    }
  }
}

void push_mock_data()
{
  (*next_ranks)[mypid] = 12;
  for (int pid = 0; pid < numprocs; pid++)
  {
    if (pid == mypid)
    {
      num_incoming_nodes_by_pid[pid] = 0;
      continue;
    }
    out_facing_nodes_by_pid[pid].push_back(mypid);
    num_incoming_nodes_by_pid[pid] = 1;
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
    rank rank = (sum * alpha + (1 - alpha));
    sigma += std::abs(node->num_outgoing_edges * (*curr_ranks)[node->id] - rank);
    (*next_ranks)[node->id] = rank / node->num_outgoing_edges;
  }
  return sigma;
}

int communicate_sigma(double sigma)
{
  if (mypid != 0)
  {
    MPI_Ssend(&sigma, sizeof(double), MPI_CHAR, 0, 1, MPI_COMM_WORLD);
    int cont;
    MPI_Status stat;
    MPI_Recv(&cont, sizeof(int), MPI_CHAR, 0, 1, MPI_COMM_WORLD, &stat);
    return cont;
  }
  else
  {
    double sum_sigmas = sigma;
    for (int pid = 1; pid < numprocs; pid++)
    {
      double recv_sigma;
      MPI_Status stat;
      MPI_Recv(&recv_sigma, sizeof(double), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);
      sum_sigmas += recv_sigma;
    }
    int cont = sum_sigmas < 1e-6;
    for (int pid = 1; pid < numprocs; pid++)
    {
      MPI_Ssend(&cont, sizeof(int), MPI_CHAR, pid, 1, MPI_COMM_WORLD);
    }
    return cont;
  }
}

template <typename T, typename K>
std::array<node_pair, 5> find_top_five(T *t, K get_pair)
{
  std::array<node_pair, 5> maximums;
  std::fill(maximums.begin(), maximums.end(), std::pair(-1, -1));

  for (auto &&raw : *t)
  {
    node_pair pair = get_pair(raw);
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
      [](Node & node) -> auto{ return std::pair(node.id, (*curr_ranks)[node.id] * node.num_outgoing_edges); });

  if (mypid != 0)
  {
    MPI_Ssend(&local_top_five, sizeof(local_top_five), MPI_CHAR, 0, 1, MPI_COMM_WORLD);
  }
  else
  {
    node_pair *candidates = new node_pair[5 * numprocs];
    std::copy(local_top_five.begin(), local_top_five.end(), candidates);
    for (int pid = 0; pid < numprocs; pid++)
    {
      MPI_Status stat;
      MPI_Recv(candidates + 5 * pid, sizeof(local_top_five), MPI_CHAR, pid, 1, MPI_COMM_WORLD, &stat);
    }
    auto global_top_five = find_top_five(candidates, [](auto candidate) -> auto{return candidate});
    std::cout << global_top_five << "\n";
  }
}

int main(int argc, char *argv[])
{
  int src;
  int dest;
  char *sdata;
  char *rdata;
  int count;
  MPI_Status stat;

  MPI_Init(&argc, &argv); /* starts MPI */

  MPI_Comm_rank(MPI_COMM_WORLD, &mypid);    /* get current process id */
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs); /* get number of processes */

  curr_ranks = new std::unordered_map<node_id, rank>;
  next_ranks = new std::unordered_map<node_id, rank>;
  init_vars();
  push_mock_data();

  int cont = 1;
  while (cont)
  {
    sync_ranks();
    double sigma = calculate_ranks();
    cont = communicate_sigma(sigma);
    std::swap(curr_ranks, next_ranks);
    next_ranks->clear();
  }

  for (auto &&item : *next_ranks)
  {
    std::cout << item.first << ": " << item.second << "\n";
  }

  MPI_Finalize();
  return 0;
}
