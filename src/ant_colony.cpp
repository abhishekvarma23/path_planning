/**
* @file ant_colony.cpp
* @author vss2sn
* @brief Contains the Ant and Ant Colony class
*/

#include <chrono>
#include <climits>
#include <cmath>
#include <random>
#include <thread>

#include "ant_colony.hpp"

Ant::Ant(Node start, int id){
	this->id_ = id;
  this->current_node_ = start;
  this->previous_node_ = Node(-1,-1);
}

AntColony::AntColony(const int n_ants, const double alpha, const double beta, const double evap_rate, const int iterations, const double Q){
  this->n_ants_ = n_ants;
  this->alpha_ = alpha;
  this->beta_ = beta;
  this->evap_rate_ = evap_rate;
  this->iterations_ = iterations;
  this->ants_ = std::vector<Ant>(n_ants_);
	this->Q_ = Q;
}

#ifdef CUSTOM_DEBUG_HELPER_FUNCION
void AntColony::PrintAntPath(Ant& ant) const {
	for(size_t k=1;k<ant.path_.size();k++) ant.path_[k].pid_ = ant.path_[k-1].id_;
	ant.path_.back().id_ = ant.path_.back().x_*grid_size_ + ant.path_.back().y_;
	auto grid_2 = grid_;
	PrintPath(ant.path_, start_, ant.path_.back(), grid_2);
	std::this_thread::sleep_for (std::chrono::seconds(1));
}
#endif

void AntColony::RemoveLoop(Ant& ant) const {
	for(auto it=ant.path_.begin(); it!=ant.path_.end();++it){
		if(*it==ant.current_node_){
			ant.steps_ = ant.path_.end() - it;
			ant.path_.erase(it, ant.path_.end());
			break;
		}
	}
}

std::vector<Node> AntColony::ant_colony(std::vector<std::vector<int>>& grid, const Node& start, const Node& goal){
  this->grid_ = grid;
	this->start_ = start; // Make sure start has id
	this->goal_ = goal;
	grid_size_ = grid_.size();
	Node c;
	motions_ = GetMotion();
	for(int i=0;i<grid_size_;i++){
		for(int j=0;j<grid_size_;j++){
			for(auto& motion : motions_){
				c = Node(i,j) + motion;
				if(c.x_ >=0 && c.x_ < grid_size_ && c.y_ >=0 && c.y_ < grid_size_) pheromone_edges_.insert({std::make_pair(i*grid_size_ + j, c.x_*grid_size_ + c.y_) , 1.0});
			}
		}
	}
	this->max_steps_ = std::pow(grid_size_,2)/2+grid_size_;

  std::random_device device;
  std::mt19937 engine(device());
  std::vector<Node> last_best_path; // saves best path of last iteration. Not over all.
  Node possible_position;

  for(int i=0;i<iterations_;i++){
    for(int j=0;j<n_ants_;j++){
			// Can assign a thread to each ant if parallel required
      Ant ant(start_, j);
      while(ant.current_node_!=goal_ && ant.steps_ < max_steps_){
        ant.path_.push_back(ant.current_node_);

				// Get next position
        std::vector<Node> possible_positions;
        std::vector<double> possible_probabilities;
        float prob_sum = 0;
        int n_obs = 0;
        for(const auto& m : motions_){
          possible_position = ant.current_node_ + m;
					possible_position.id_ = possible_position.x_*grid_size_ + possible_position.y_;
          if(possible_position.x_ >=0 && possible_position.x_ < grid_size_ && possible_position.y_ >=0 && possible_position.y_ < grid_size_
             && possible_position!=ant.previous_node_){
            possible_positions.push_back(possible_position);
            double new_prob = std::pow(pheromone_edges_[std::make_pair(possible_position.id_, ant.current_node_.id_)], alpha_) *
              std::pow(1.0/std::pow(std::pow((possible_position.x_ - goal_.x_),2) +std::pow((possible_position.y_ - goal_.y_),2),0.5), beta_);
						if(grid_[possible_position.x_][possible_position.y_]==1){
							n_obs+=1;
							new_prob = 0;
						}
            possible_probabilities.push_back(new_prob);
            prob_sum += new_prob;
          }
        }
        if(n_obs==static_cast<int>(possible_positions.size())) break;// Ant in a cul-de-sac
        else if(prob_sum == 0){
          double new_prob = 1.0/(static_cast<int>(possible_positions.size())-n_obs);
          for(size_t i=0;i<possible_positions.size();i++){
            if(grid_[possible_positions[i].x_][possible_positions[i].y_]==0) possible_probabilities[i]=new_prob;
            else possible_probabilities[i]=0;
          }
        }
        else for(auto& p : possible_probabilities) p/=prob_sum;
        std::discrete_distribution<> dist(possible_probabilities.begin(), possible_probabilities.end());
        ant.previous_node_ = ant.current_node_;
        ant.current_node_ = possible_positions[dist(engine)];

				// Removing any loops if reached previously reached point
				// TODO: add check to count number of loops removed and stop if going into inf
				RemoveLoop(ant);

        ant.steps_++;
      }
			// If goal found, add to path
      if(ant.current_node_==goal_){
				ant.current_node_.id_ = ant.current_node_.x_*grid_size_ + ant.current_node_.y_;
        ant.path_.push_back(ant.current_node_);
        ant.found_goal_ = true;
      }
      ants_[j] = ant;
    }

		// Pheromone deterioration
    for(auto it = pheromone_edges_.begin(); it!=pheromone_edges_.end();it++) it->second = it->second*(1-evap_rate_);

    int bpl = INT_MAX;
    std::vector<Node> bp;

		// Pheromone update based on successful ants
    for(const Ant& ant : ants_){
			// PrintAntPath(ant);
      if(ant.found_goal_){ // Use iff goal reached
        if(static_cast<int>(ant.path_.size()) < bpl){ // Save best path yet in this iteration
          bpl = ant.path_.size();
          bp = ant.path_;
        }
        double c = Q_/(ant.path_.size()-1); //c = cost / reward. Reward here, increased pheromone
        for(size_t i=1; i < ant.path_.size(); i++){ // Assuming ant can tell which way the food was based on how the phermones detereorate. Good for path planning as prevents moving in the opposite direction to path and improves convergence
          auto it = pheromone_edges_.find(std::make_pair(ant.path_[i].id_, ant.path_[i-1].id_));
          it->second+= c;
        }
      }
    }
    if(i+1==iterations_) last_best_path = bp;
  }//for every iteration loop ends here
	if(last_best_path.empty()){
		last_best_path.clear();
		Node no_path_node(-1,-1,-1,-1,-1,-1);
		last_best_path.push_back(no_path_node);
		return last_best_path;
	}
  for(size_t i=1; i < last_best_path.size(); i++) last_best_path[i].pid_ = last_best_path[i-1].id_;
  last_best_path.back().id_ = last_best_path.back().x_*grid_size_ + last_best_path.back().y_;
  return last_best_path;
}

#ifdef BUILD_INDIVIDUAL
/**
* @brief Script main function. Generates start and end nodes as well as grid, then creates the algorithm object and calls the main algorithm function.
* @return 0
*/
int main(){
  int n = 11;
	std::vector<std::vector<int>> grid(n, std::vector<int>(n));
  MakeGrid(grid);

  std::random_device rd; // obtain a random number from hardware
  std::mt19937 eng(rd()); // seed the generator
  std::uniform_int_distribution<int> distr(0,n-1); // define the range

  Node start(distr(eng),distr(eng),0,0,0,0);
  Node goal(distr(eng),distr(eng),0,0,0,0);

  start.id_ = start.x_ * n + start.y_;
  start.pid_ = start.x_ * n + start.y_;
  goal.id_ = goal.x_ * n + goal.y_;
  start.h_cost_ = abs(start.x_ - goal.x_) + abs(start.y_ - goal.y_);
  //Make sure start and goal are not obstacles and their ids are correctly assigned.
  grid[start.x_][start.y_] = 0;
  grid[goal.x_][goal.y_] = 0;
  PrintGrid(grid);

	//Normally as  beta increases the solution becomes greedier. However, as the heuristic is < 1 here, reducing beta increases the value places on the heuristic
  AntColony new_ant_colony(1, 1, 0.5, 0.3, 5, 10.0);
  std::vector<Node> path_vector = new_ant_colony.ant_colony(grid, start, goal);
  PrintPath(path_vector, start, goal, grid);
  return 0;
}
#endif  // BUILD_INDIVIDUAL
