#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <string.h>

typedef struct stepsize_t {
	double x;
	double y;
	double t;
} Stepsize;

typedef struct grid_t {
	double* internal_storage;
	int len_x;
	int len_y;
} Grid;

typedef struct grid_options_t {
	size_t len_x;
	size_t len_y;
	size_t len_t;
} GridOptions;

#define grid_element(g,i,j) g.internal_storage[(i * g.len_x) + (j - 1)]

Grid alloc_grid(int len_x, int len_y) {
	Grid g;
	g.len_x = len_x;
	g.len_y = len_y;
	g.internal_storage = (double*)malloc(sizeof(double) * len_x * len_y);
	return g;
}

Grid generate_initial_conditions(int len_x, int len_y) {
	Grid initial_conditions = alloc_grid(len_x, len_y);
	int i,j;
	double hx = 1.0 / ((double) len_x);
	for (i = 0; i < len_x; i++) {
		for (j = 0; j < len_y; j++) {
			grid_element(initial_conditions,i,j) = sin(i * hx);
		}
	}
	return initial_conditions;
}

void solve_interior(Grid current, Grid previous, Stepsize h) {
	int i,j;
	int max_i = current.len_x - 2;
	int max_j = current.len_y - 2;
	for(i = 1; i < max_i; i++) {
		for(j = 1; j < max_j; j++) {
			double uijn = grid_element( previous,i,j);
			double t_contribution = uijn;

			double uiP1jn = grid_element( previous,i+1,j);
			double uiM1jn = grid_element(previous,i-1,j);
			double x_contribution = h.t * (uiP1jn - 2*uijn + uiM1jn) / (h.x * h.x);

			double uijP1n = grid_element(previous,i,j+1);
			double uijM1n = grid_element(previous,i,j-1);
			double y_contribution = h.t * (uijP1n - 2*uijn + uijM1n) / (h.y * h.y);

			double uijnP1 = t_contribution + x_contribution + y_contribution;
			grid_element(current,i,j) = uijnP1;	
		}
	}
} 

void apply_boundary_conditions(Grid g) {
	int i,j;
	// North Boundary
	for(i=0; i < g.len_x; i++) grid_element(g,i,0) = 0;

	// South Boundary
	for(i=0; i < g.len_x; i++) grid_element(g,i,g.len_y - 1) = 0;

	// East Boundary
	for(j=0; j < g.len_y; j++) grid_element(g,0,j) = 0;

	// West Boundary
	for(j=0; j < g.len_y; j++) grid_element(g,g.len_x - 1,j) = 0;
}

void store_grid(Grid g) {
	FILE* gridFile = fopen("output.otj_grid","w");
	size_t num_cells = g.len_x * g.len_y;
	size_t cell_size = sizeof(double);
	size_t elements_written = fwrite(g.internal_storage, cell_size, num_cells, gridFile);
	if (elements_written < num_cells) {
		printf("An error occurred while saving the grid.\n");
	}
}
	
typedef struct timing_measurement_t {
	double beginning;
	double end;
	char* message;
} TimingMeasurement;

TimingMeasurement start_timer(char* message) {
	TimingMeasurement tm;
	tm.message = message;
	tm.beginning = MPI_Wtime();
	tm.end = tm.beginning;
	return tm;
}

#define stop_timer(tm) printf("[Timer] %s: %f seconds\n", tm.message, MPI_Wtime() - tm.beginning); 

GridOptions parse_grid_options(int argc, char** argv) {
	GridOptions go;
	// Set defaults
	go.len_x = 500;
	go.len_y = 500;
	go.len_t = 10;

	int i = 1;
	while(i < argc - 1) {
		if (strcmp(argv[i],"-x") == 0) {
			go.len_x = atoi(argv[i+1]);
			i += 2;
		} else if (strcmp(argv[i],"-y") == 0) {
			go.len_y = atoi(argv[i+1]);
			i += 2;
		} else if (strcmp(argv[i],"-t") == 0) {
			go.len_t = atoi(argv[i+1]);
			i += 2;
		} else {
			printf("I'm sorry, I don't recognize the %s option.\n", argv[i]);
			printf("Options:\n");
			printf("\t-xX : Sets width of grid to X in the x direction\n");
			printf("\t-yY : Sets width of grid to Y in the y direction\n");
			printf("\t-tT : Sets number of timesteps to T\n");
			exit(1);
		}
	}
	return go;
}

Stepsize stepsize_from_grid_options(GridOptions go) {
	Stepsize h;
	h.x = 1.0 / ((double) go.len_x);
	h.y = 1.0 / ((double) go.len_y);
	h.t = 1.0 / ((double) go.len_t);
	return h;
}

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv);
	GridOptions go = parse_grid_options(argc, argv);

	TimingMeasurement tm = start_timer("Initial Conditions");
	Grid initial_conditions = generate_initial_conditions(go.len_x, go.len_y);
	stop_timer(tm);

	Stepsize h = stepsize_from_grid_options(go);
	Grid grids_by_timestep[go.len_t];
	grids_by_timestep[0] = initial_conditions;

	tm = start_timer("Solve Problem");
	int tau;
	for(tau = 1; tau < go.len_t; tau++) {
		grids_by_timestep[tau] = alloc_grid(go.len_x, go.len_y);
		apply_boundary_conditions(grids_by_timestep[tau]);
		solve_interior(grids_by_timestep[tau], grids_by_timestep[tau - 1],h);
	}
	stop_timer(tm);

	tm = start_timer("Store Grid");
	store_grid(grids_by_timestep[go.len_t - 1]);
	stop_timer(tm);
	
	return 0;
}
