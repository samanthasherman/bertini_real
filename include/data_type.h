#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <gmp.h>
#include <time.h>
#include <float.h>
#include <limits.h>
#include <mpfr.h>
#include <mpf2mpfr.h>




#ifndef _DATA_TYPE_H
#define _DATA_TYPE_H

#include "fileops.h"
#include "cascade.h"
#include "polysolve.h"


/*** low-level data types. ***/




//typedef struct
//{
//  int num_pts;
//  point_d *pts;
//} witness_point_set_d;


typedef struct
{
  char *func;  //symbolic representation of function (straight from input file).
} function;


//typedef struct
//{
//  function *funcs; //probably not used for now.
//  //prog_t slp; //SLP -- we'll use this primarily, at least at first.
//  char *fName; //system can be given in a file -- this is its name.
//}system;

////begin mp types
//typedef struct
//{
//  int num_pts;
//  point_mp *pts;
//} witness_point_set_mp;
////end mp types


typedef struct
{
//  system sys;
  vec_d *L;
	vec_d *patch;
  point_d *pts_d;
	
	vec_mp *L_mp;
	vec_mp *patch_mp;
  point_mp *pts_mp;
	
	
	
	int codim;
  int comp_num;
	
	
	int incidence_number;
	int num_pts;
	int num_var_gps;
	int num_variables;
	int num_linears;
	int num_patches;
	int patch_size;
	int MPType; //  will indicate the type of the solve.  both fields will have data, but one will be used.
	
	char ** variable_names;
} witness_set;  //For a single irreducible component!  Need num. irred. decomp. type later.
// end the double types




// CURVE CELL DECOMP DATA TYPES
typedef struct
{
  point_d pt;
  point_mp pt_mp;
  comp_d projVal; //Value of projection pi applied to pt; used for easy comparison of projected values later....
  comp_mp projVal_mp;
  int type;  //See enum below.
  int num_left;  //this and next line track how often this vertex appears in
  int num_right; //edges;  good for sanity check AND determining V0.
}vertex;

//The following lets us use words instead of numbers to indicate vertex type.
enum {CRITICAL=0, NEW=1, MIDPOINT=2, ISOLATED=-1};


typedef struct
{
  int left;  //index into vertices
  int right; //index into vertices
	int midpt; // index into vertices
} edge;

typedef struct
{
	int num_variables;
	
  vertex *vertices;  //Isolated real points.
  edge *edges;
	
	//these counters keep track of the number of things
	int			 num_vertices;
	int      num_edges;
	
  int      num_V0;
  int      num_V1;
  int      num_midpts;
	int			 num_new;

	// these arrays of integers index the points.
	int			*V0_indices;
	int			*V1_indices;
	int			*midpt_indices;
	int			*new_indices;

	vec_mp	pi; // the projection
	vec_d		pi_d; // the projection
	mat_mp n_minus_one_randomizer_matrix;
	
} curveDecomp_d;


typedef struct
{
  vec_d    **vertices;  //points
  vec_d    *projection_values; //projection of points
  vec_mp   **vertices_mp;
  vec_mp   *projection_values_mp;
  int      **refine;
  int      num_edges;// number of edges
  int      *num_pts;// number of points
}sample_d;





//int index_in_V1(curveDecomp_d *C, vec_mp testpoint, comp_mp projection_value, tracker_config_t T, int sidedness);
//void add_point_to_V0(curveDecomp_d *C, vertex new_vertex);
//void add_point_to_V1(curveDecomp_d *C, vertex new_vertex);

int add_vertex(curveDecomp_d *C, vertex new_vertex);

int index_in_vertices(curveDecomp_d *C, vec_mp testpoint, comp_mp projection_value, tracker_config_t T, int sidedness);

void init_vertex_d( vertex *curr_vertex);
void init_vertex_mp(vertex *curr_vertex);
void cp_vertex_mp(vertex *target_vertex, vertex new_vertex);

void init_edge(edge *curr_edge);
//void init_edge_mp(edge *curr_edge, int num_variables);
//void init_edge_d(edge *curr_edge, int num_variables);

void add_edge(curveDecomp_d *C, edge new_edge);


//function prototypes for bertini_real data clearing etc.
void init_curveDecomp_d(curveDecomp_d *C);
void merge_witness_sets(witness_set *W_out,witness_set W_left,witness_set W_right);

void init_variable_names(witness_set *W, int num_vars);

void cp_names(witness_set *W_out, witness_set W_in);
void cp_linears(witness_set *W_out, witness_set W_in);
void cp_patches(witness_set *W_out, witness_set W_in);
void cp_witness_set(witness_set *W_out, witness_set W_in);
void init_witness_set_d(witness_set *W);

void norm_of_difference(mpf_t result, vec_mp left, vec_mp right);

void dehomogenize_d(vec_d *result, vec_d dehom_me);
void dehomogenize_mp(vec_mp *result, vec_mp dehom_me);

void dot_product_d(comp_d result, vec_d one, vec_d two);
void dot_product_mp(comp_mp result, vec_mp one, vec_mp two);

void projection_value_homogeneous_input_d(comp_d result, vec_d input, vec_d projection);
void projection_value_homogeneous_input(comp_mp result, vec_mp input, vec_mp projection);

int isSamePoint_homogeneous_input_d(point_d left, point_d right);

int isSamePoint_homogeneous_input_mp(point_mp left, point_mp right);

void write_homogeneous_coordinates(witness_set W, char filename[]);
void write_dehomogenized_coordinates(witness_set W, char filename[]);
void write_linears(witness_set W, char filename[]);

void clear_curveDecomp_d(curveDecomp_d *C, int MPType);
void clear_sample_d(sample_d *S, int MPType);
void clear_witness_set(witness_set W);
void print_witness_set_to_screen(witness_set W);
void print_point_to_screen_matlab(vec_d M, char name[]);
void print_point_to_screen_matlab_mp(vec_mp M, char name[]);
void print_matrix_to_screen_matlab(mat_d M, char name[]);
void print_matrix_to_screen_matlab_mp(mat_mp M, char name[]);

void print_comp_mp_matlab(comp_mp M,char name[]);


void print_path_retVal_message(int retVal);

/**
retrieves the number of variables from the PPD by taking the sum of the sizes, plus the sum of the types.
*/
int get_num_vars_PPD(preproc_data PPD);




#endif

