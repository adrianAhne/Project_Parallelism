#include "projet.h"
#include <sys/time.h>
/* 2017-02-23 : version 1.0 */

unsigned long long int node_searched = 0;

// measure time
double my_gettimeofday(){
  struct timeval tmp_time;
  gettimeofday(&tmp_time, NULL);
  return tmp_time.tv_sec + (tmp_time.tv_usec * 1.0e-6L);
}


void evaluate(tree_t * T, result_t *result)
{
	node_searched++;
	
	move_t moves[MAX_MOVES];
	int n_moves;

	result->score = -MAX_SCORE - 1;
	result->pv_length = 0;
	
	if (test_draw_or_victory(T, result))
		return;

				if (TRANSPOSITION_TABLE && tt_lookup(T, result))     /* la réponse est-elle déjà connue ? */
	return;
	
	compute_attack_squares(T);

				/* profondeur max atteinte ? si oui, évaluation heuristique */
	if (T->depth == 0) {
		result->score = (2 * T->side - 1) * heuristic_evaluation(T);
		return;
	}
	
	n_moves = generate_legal_moves(T, &moves[0]);

	/* absence de coups légaux : pat ou mat */
	if (n_moves == 0) {
		result->score = check(T) ? -MAX_SCORE : CERTAIN_DRAW;
		return;
	}
	
	if (ALPHA_BETA_PRUNING)
		sort_moves(T, n_moves, moves);



    if (T->height < 2)
    {
        //#pragma omp parallel
        //#pragma omp single nowait
        for (int i = 0; i < n_moves; i++) {

        	
    		tree_t child;
            result_t child_result;
              


            #pragma omp task firstprivate(T) private(child, child_result) shared(result)
            {

            	// writes in child the obtaining move by playing moves[i]
            	play_move(T, moves[i], &child);
                
                evaluate(&child, &child_result);


                int child_score = -child_result.score;

                #pragma omp critical
                {
	                // if obtained score is better than existing score, replace it
	                if (child_score > result->score) {
	                    result->score = child_score;
	                    result->best_move = moves[i];
	                    result->pv_length = child_result.pv_length + 1;
	                    
	                    for(int j = 0; j < child_result.pv_length; j++)
	                        result->PV[j+1] = child_result.PV[j];
	                    
	                    result->PV[0] = moves[i];

                	}  

	                T->alpha = MAX(T->alpha, child_score); 

				}

            }

            // break is not allowed in basic parallelisme
            //if (ALPHA_BETA_PRUNING && child_score >= T->beta)
            //    break;  

        }

        #pragma omp taskwait

        if (TRANSPOSITION_TABLE)
            tt_store(T, result);
 
	} 
	else {
		for (int i = 0; i < n_moves; i++) {
			tree_t child;
			result_t child_result;

			play_move(T, moves[i], &child);
			
			evaluate(&child, &child_result);
			
			int child_score = -child_result.score;

			if (child_score > result->score) {
				result->score = child_score;
				result->best_move = moves[i];
				result->pv_length = child_result.pv_length + 1;
				for(int j = 0; j < child_result.pv_length; j++)
					result->PV[j+1] = child_result.PV[j];
				result->PV[0] = moves[i];
			}

			if (ALPHA_BETA_PRUNING && child_score >= T->beta)
				break;    

			T->alpha = MAX(T->alpha, child_score);
		}
	}
	

	if (TRANSPOSITION_TABLE)
		tt_store(T, result);
}


void decide(tree_t * T, result_t *result)
{
	for (int depth = 1;; depth++) {
		T->depth = depth;
		T->height = 0;
		T->alpha_start = T->alpha = -MAX_SCORE - 1;
		T->beta = MAX_SCORE + 1;

		printf("=====================================\n");

        #pragma omp parallel
        #pragma omp single nowait
		evaluate(T, result);

		printf("depth: %d / score: %.2f / best_move : ", T->depth, 0.01 * result->score);
		print_pv(T, result);
		
		if (DEFINITIVE(result->score))
			break;
	}
}

int main(int argc, char **argv)
{  
	tree_t root;
	result_t result;

	/* Time measure */
	double startTime, endTime;

    omp_set_nested(1);


	if (argc < 2) {
		printf("usage: %s \"4k//4K/4P w\" (or any position in FEN)\n", argv[0]);
		exit(1);
	}

	if (ALPHA_BETA_PRUNING)
		printf("Alpha-beta pruning ENABLED\n");

	if (TRANSPOSITION_TABLE) {
		printf("Transposition table ENABLED\n");
		init_tt();
	}
	
	parse_FEN(argv[1], &root);
	print_position(&root);
	
	/* start time measure */
	startTime = my_gettimeofday();   
	
	decide(&root, &result);

	/* end time of measure */
	endTime = my_gettimeofday();

	printf("\nTemps total de calcul : %g sec\n", endTime - startTime);

	printf("\nDécision de la position: ");
	switch(result.score * (2*root.side - 1)) {
		case MAX_SCORE: printf("blanc gagne\n"); break;
		case CERTAIN_DRAW: printf("partie nulle\n"); break;
		case -MAX_SCORE: printf("noir gagne\n"); break;
		default: printf("BUG\n");
	}

	printf("Node searched: %llu\n", node_searched);
	
	if (TRANSPOSITION_TABLE)
		free_tt();
	return 0;
}
