#include "sampler.hpp"











int main(int argC, char *args[])
{
	
	boost::timer::auto_cpu_timer t;
	
	
	MPI_Init(&argC,&args);
	
	
	boost::filesystem::path inputName, witnessSetName, samplingNamenew;
	
	
	
	
	
	
	
	sampler_configuration sampler_options;
	
	sampler_options.splash_screen();
	sampler_options.parse_commandline(argC, args);
    
	
	
	int MPType, dimension;
	
	boost::filesystem::path directoryName;
	
	get_dir_mptype_dimen( directoryName, MPType, dimension);
	
	
	
	vertex_set V;
	V.setup_vertices(directoryName / "V.vertex"); //setup V structure from V.vertex
	
    
	
	witnessSetName = directoryName / "witness_set";
	samplingNamenew = directoryName;
	
	
	solver_configuration solve_options;
	
	
	//only one will be used.  i don't know how to avoid this duplicity.
	curve_decomposition C;
	surface_decomposition surf_input;
	
	decomposition * decom_pointy; // this feels unnecessary
	switch (dimension) {
		case 1:
		{
			C.setup(directoryName);
			decom_pointy = &C;
			
		}
			break;
			
		case 2:
		{
			surf_input.setup(directoryName);
			decom_pointy = &surf_input;
			
		}
			break;
			
		default:
		{
			std::cout << "sampler not capable of sampling anything but dimension 1 or 2.  this is of dim " << dimension << std::endl;
			return 0;
		}
			break;
	}
	
	
	
	
	common_sampler_startup(*decom_pointy,
						   sampler_options,
						   solve_options);// this does nothing so far.  want to abstract it.
	
	
	
	
	
	/////////
	////////
	//////
	////
	//
	//  Generate new sampling data
	//
    

	switch (dimension) {
		case 1:
		{
			if (sampler_options.use_fixed_sampler) {
				C.fixed_sampler(V,
								sampler_options,
								solve_options,
								sampler_options.target_num_samples);
			}
			else{
				C.adaptive_sampler(V,
								   sampler_options,
								   solve_options);
			}
			
			
			
			//output
			C.output_sampling_data(directoryName);
			
			V.print(directoryName / "V_samp.vertex");
			
			
		}
			break;
			
		case 2:
		{
			surf_input.fixed_sampler(V,
									 sampler_options,
									 solve_options);
			
			
			surf_input.output_sampling_data(directoryName);
			
			V.print(directoryName / "V_samp.vertex");
			
			
		}
			break;
		default:
			break;
	}
    
	
	
	
    
	//
	//   Done with the main call
	////
	/////
	///////
	////////
	
	
	
	
	
    
    
	
	

	
	clearMP();
	MPI_Finalize();
	
	return 0;
}



void common_sampler_startup(const decomposition & D,
							sampler_configuration & sampler_options,
							solver_configuration & solve_options)
{
	
	parse_input_file(D.input_filename); // restores all the temp files generated by the parser, to this folder.  i think this can be removed?  but the PPD and tracker config both depend on it...  so maybe not.
	
    
	
	// set up the solver configuration
	get_tracker_config(solve_options, solve_options.T.MPType);
	
	initMP(solve_options.T.Precision);
	
	
	parse_preproc_data("preproc_data", &solve_options.PPD);
    
	
	
	
	
	solve_options.verbose_level = sampler_options.verbose_level;
	if (solve_options.verbose_level>=2) {
		solve_options.show_status_summary=1;
	}
	
	
	
	
	
	
	solve_options.T.ratioTol = 0.99999999; // manually assert to be more permissive.  i don't really like this.
	
	
	
	solve_options.use_midpoint_checker = 0;
	solve_options.use_gamma_trick = 0;
	solve_options.allow_singular = 1;
    solve_options.robust = true;
	
}


void curve_decomposition::adaptive_sampler(vertex_set & V,
											  sampler_configuration & sampler_options,
											  solver_configuration & solve_options)
{
    witness_set W;
	
	
	parse_input_file(input_filename); // restores all the temp files generated by the parser, to this folder.
	solve_options.get_PPD();
	
	W.input_filename = input_filename;
	W.num_variables = num_variables;
	W.num_synth_vars = num_variables - V.num_natural_variables;
    
	W.get_variable_names();
	
	
	
	W.reset_patches(); // is this necessary?  i don't think so, only if we actually parse from file and get patches as a result.  then again, it probably doesn't hurt.
	for (int ii=0; ii<num_patches; ii++) {
		W.add_patch(patch[ii]);
	}
	
	
	
	mat_mp randomizer_matrix; init_mat_mp2(randomizer_matrix, 0,0, solve_options.T.AMP_max_prec);
	
	//create the vector of integers
	std::vector< int > randomized_degrees;
	
	//get the matrix and the degrees of the resulting randomized functions.
	make_randomization_matrix_based_on_degrees(randomizer_matrix, randomized_degrees, W.num_variables-W.num_patches-1, solve_options.PPD.num_funcs);
	
	
	
	
	
	
	adaptive_set_initial_sample_data();
	
	
	V.set_curr_projection(pi[0]);
	V.set_curr_input(W.input_filename);
	
	
	
	int	num_vars = num_variables;
	
	
	witness_set Wnew;
	
	
	vec_mp target_projection;
	init_vec_mp(target_projection,num_vars); target_projection->size = num_vars;
	vec_cp_mp(target_projection,pi[0]); // copy the projection into target_projection
	
	
	vec_mp startpt;
	init_vec_mp(startpt,num_vars); startpt->size = num_vars;
	
	
	vec_mp start_projection;
	init_vec_mp(start_projection,num_vars);  start_projection->size = num_vars;
	vec_cp_mp(start_projection,pi[0]); // grab the projection, copy it into start_projection
	

	
	
	comp_mp temp, temp1, target_projection_value;
	init_mp(temp);  init_mp(temp1); init_mp(target_projection_value);
    
	int prev_num_samp, sample_counter;
	std::vector<bool> refine_current, refine_next;
	
	
	


	
	vertex temp_vertex;
    
	mpf_t dist_away; mpf_init(dist_away);
	int interval_counter;
	int num_refinements;
	std::vector<int> current_indices;
	
	multilin_config ml_config(solve_options,randomizer_matrix);
	
	std::cout << num_edges << std::endl;
	
	
	for(int ii=0;ii<num_edges;ii++) // for each of the edges
	{
        
		adaptive_set_initial_refinement_flags(num_refinements,
                                     refine_current,
                                     current_indices,
                                     V,
                                     ii, sampler_options);
		
		prev_num_samp = num_samples_each_edge[ii]; // grab the number of points from the array of integers
		
		
		
		int pass_number  = 0;//this should be the only place this is reset.
		while(1) // breaking condition is all samples being less than TOL away from each other (in the infty norm sense).
		{
            
			refine_next.resize(prev_num_samp+num_refinements-1);; // allocate refinement flag
            
			std::vector<int> new_indices;
			new_indices.resize(prev_num_samp+num_refinements);
			
			new_indices[0] = current_indices[0];
			sample_counter = 1; // this will be incremented every time we put a point into new_points
								// starts at 1 because already committed one.
								// this should be the only place this counter is reset.
			
			
			if (sampler_options.verbose_level>=0)
				printf("edge %d, pass %d, %d refinements\n",ii,pass_number,num_refinements);
			
			if (sampler_options.verbose_level>=1) {
				printf("the current indices:\n");
				for (int jj=0; jj<prev_num_samp; jj++)
					printf("%d ",current_indices[jj]);
				printf("\n\n");
			}
			
			if (sampler_options.verbose_level>=1) {
				printf("refine_flag:\n");
				for (int jj=0; jj<prev_num_samp-1; jj++) {
                    printf("%s ",refine_current[jj]?"1":"0");
				}
				printf("\n\n");
			}
			
            
			
			num_refinements = 0; // reset this counter.  this should be the only place this is reset
			interval_counter = 0;
			for(int jj=0; jj<prev_num_samp-1; jj++) // for each interval in the previous set
			{
				
				
				
				if (sampler_options.verbose_level>=2)
					printf("interval %d of %d\n",jj,prev_num_samp-1);
				
				
				
				int startpt_index; int left_index; int right_index;
				
				// set the starting projection and point.
				if(jj==0){// if on the first sample, go the right
					startpt_index = current_indices[1]; //right!
				}
				else{ //go to the left
					startpt_index = current_indices[jj]; // left!
				}
				
				left_index = current_indices[jj];
				right_index = current_indices[jj+1];
				
				
				if (new_indices[sample_counter-1]!=left_index) {
					printf("index mismatch\n");
					exit(1);
				}
				
                
				
				if(refine_current[jj]==1) //
				{
                    
					vec_cp_mp(startpt,V.vertices[startpt_index].pt_mp);
					set_mp(&(start_projection->coord[0]), &V.vertices[startpt_index].projection_values->coord[0]);
					neg_mp(&(start_projection->coord[0]), &(start_projection->coord[0]));
					
					
					estimate_new_projection_value(target_projection_value,				// the new value
                                                  V.vertices[left_index].pt_mp,	//
                                                  V.vertices[right_index].pt_mp, // two points input
                                                  pi[0]);												// projection (in homogeneous coordinates)
					
					
                    
					neg_mp(&target_projection->coord[0],target_projection_value); // take the opposite :)
					
					
					set_witness_set_mp(&W, start_projection,startpt,num_vars); // set the witness point and linear in the input for the lintolin solver.
					
                    
					if (sampler_options.verbose_level>=3) {
						print_point_to_screen_matlab(W.pts_mp[0],"startpt");
						print_comp_matlab(&W.L_mp[0]->coord[0],"initial_projection_value");
						print_comp_matlab(target_projection_value,"target_projection_value");
					}
					
					if (sampler_options.verbose_level>=5)
						W.print_to_screen();
					
					if (sampler_options.verbose_level>=10) {
						mypause();
					}
					
					
					
					multilin_solver_master_entry_point(W,         // witness_set
                                                       &Wnew, // the new data is put here!
                                                       &target_projection,
                                                       ml_config,
                                                       solve_options);
					
					
					if (sampler_options.verbose_level>=3)
						print_point_to_screen_matlab(Wnew.pts_mp[0], "new_solution");
					
					
					
					
					
					// check how far away we were from the LEFT interval point
					norm_of_difference(dist_away,
                                       Wnew.pts_mp[0], // the current new point
                                       V.vertices[left_index].pt_mp);// jj is left, jj+1 is right
					
					if ( mpf_cmp(dist_away, sampler_options.TOL )>0 ){
						refine_next[interval_counter] = true;
						num_refinements++;
					}
					else{
                        refine_next[interval_counter] = false;
					}
					interval_counter++;
					
					
					
					
					// check how far away we were from the RIGHT interval point
					norm_of_difference(dist_away,
                                       Wnew.pts_mp[0], // the current new point
                                       V.vertices[right_index].pt_mp);
					
					if (mpf_cmp(dist_away, sampler_options.TOL ) > 0){
						refine_next[interval_counter] = 1;
						num_refinements++;
					}
					else{
						refine_next[interval_counter] = 0;
					}
					interval_counter++;
					
					
					vec_cp_mp(temp_vertex.pt_mp,Wnew.pts_mp[0]);
					temp_vertex.type = CURVE_SAMPLE_POINT;
					
                    
					new_indices[sample_counter] = V.add_vertex(temp_vertex);
					sample_counter++;
					
					new_indices[sample_counter] = right_index;
					sample_counter++;
                    
					Wnew.reset();
					
				}
				else {
					if (sampler_options.verbose_level>=2)
						printf("adding sample %d\n",sample_counter);
					
					refine_next[interval_counter] = 0;
					new_indices[sample_counter] = right_index;
					interval_counter++;
					sample_counter++;
				}
			}
            
			
			if (sampler_options.verbose_level>=1) // print by default
				printf("\n\n");
			
			if( (num_refinements == 0) || (pass_number >= sampler_options.maximum_num_iterations) ) // if have no need for new samples
			{
				
				if (sampler_options.verbose_level>=1) // print by default
					printf("breaking\nsample_counter = %d\n",sample_counter);
				
				
				sample_indices[ii].swap(new_indices);
				num_samples_each_edge[ii] = sample_counter;
                
				break; // BREAKS THE WHILE LOOP
			}
			else{
				
				refine_current = refine_next; // reassign this pointer
				current_indices.swap(new_indices);
                
				prev_num_samp=sample_counter; // update the number of samples
				pass_number++;
			}
            
		}//while loop
		if (sampler_options.verbose_level>=2) {
			printf("exiting while loop\n");
		}
		printf("\n");
	}  // re: ii (for each edge)
	
	
	
	
	clear_mp(temp); clear_mp(temp1);
	clear_vec_mp(start_projection);
	clear_vec_mp(target_projection);
	
	clear_mat_mp(randomizer_matrix);
    
	
	
}




void curve_decomposition::fixed_sampler(vertex_set & V,
									  sampler_configuration & sampler_options,
									  solver_configuration & solve_options,
									  int target_num_samples)
{
    //should reparse here
	
	witness_set W;
	
	
	parse_input_file(input_filename); // restores all the temp files generated by the parser, to this folder.
	
	solve_options.get_PPD();
	
	W.input_filename = input_filename;
	W.num_variables = num_variables;
	W.num_synth_vars = num_variables - V.num_natural_variables;
    
//	W.get_variable_names();
	
	for (int ii=0; ii<num_patches; ii++) {
		W.add_patch(patch[ii]);
	}
	
	
	
	mat_mp randomizer_matrix; init_mat_mp2(randomizer_matrix, 0,0, solve_options.T.AMP_max_prec);
	//create the vector of integers
	std::vector< int > randomized_degrees;
	//get the matrix and the degrees of the resulting randomized functions.
	make_randomization_matrix_based_on_degrees(randomizer_matrix, randomized_degrees, W.num_variables-W.num_patches-1, solve_options.PPD.num_funcs);
	
	
	
	
	
	
	
	fixed_set_initial_sample_data(target_num_samples);
	
	V.set_curr_projection(pi[0]);
	V.set_curr_input(W.input_filename);
	
	

	
	
	
	//where we will move to
	vec_mp target_projection; init_vec_mp2(target_projection,1,1024); target_projection->size = 1;
	vec_cp_mp(target_projection,pi[0]); // copy the projection into target_projection
	
	if (target_projection->size < W.num_variables) {
		increase_size_vec_mp(target_projection,W.num_variables); target_projection->size = W.num_variables;
		for (int ii=W.num_synth_vars+1; ii<W.num_variables; ii++) {
			set_zero_mp(&target_projection->coord[ii]);
		}
	}
	
	W.add_linear(target_projection); // grab copy the (possibly increased) projection into start_projection
	

	
	comp_mp temp, temp1, target_projection_value;
	init_mp2(temp,1024);  init_mp2(temp1,1024); init_mp2(target_projection_value,1024);
    
	
	
	vertex temp_vertex;
    witness_set Wnew; // to hold the output


	multilin_config ml_config(solve_options,randomizer_matrix);
	

	comp_mp interval_width; init_mp2(interval_width,1024); set_zero_mp(interval_width);
	comp_mp num_intervals;  init_mp2(num_intervals,1024); set_zero_mp(num_intervals);
	

	for (int ii=0;ii<num_edges;ii++) // for each of the edges
	{
		if (edges[ii].is_degenerate()) {
//			std::cout << "edge " << ii << " is degenerate" << std::endl;
			continue;
		}
		else{
			std::cout << "edge " << ii << std::endl;
		}
		
		
		neg_mp(&W.L_mp[0]->coord[0],&V.vertices[edges[ii].midpt].projection_values->coord[V.curr_projection]);
		
		W.reset_points();
		W.add_point(V.vertices[edges[ii].midpt].pt_mp);
		
		
		mpf_set_d(num_intervals->r,double(target_num_samples-1));
		
		sub_mp(interval_width,&V.vertices[edges[ii].right].projection_values->coord[V.curr_projection],&V.vertices[edges[ii].left].projection_values->coord[V.curr_projection]);
		
		div_mp(interval_width,interval_width,num_intervals);
		
		set_mp(target_projection_value,&V.vertices[edges[ii].left].projection_values->coord[V.curr_projection]);
		
		//add once to get us off 0
		add_mp(target_projection_value,target_projection_value,interval_width);
		
		for (int jj=1; jj<target_num_samples-1; jj++) {
			
			
			
			neg_mp(&target_projection->coord[0],target_projection_value); // take the opposite :)
			
			
			
			if (sampler_options.verbose_level>=3) {
				print_point_to_screen_matlab(W.pts_mp[0],"startpt");
				print_comp_matlab(&W.L_mp[0]->coord[0],"initial_projection_value");
				print_comp_matlab(target_projection_value,"target_projection_value");
			}
			
			if (sampler_options.verbose_level>=5)
				W.print_to_screen();
			
			
			multilin_solver_master_entry_point(W,         // witness_set
											   &Wnew, // the new data is put here!
											   &target_projection,
											   ml_config,
											   solve_options);
			
			
			if (Wnew.num_pts==0) {
				//TODO: ah shit!  this ain't good.  how to deal with it?
			}
			
			vec_cp_mp(temp_vertex.pt_mp,Wnew.pts_mp[0]);
			temp_vertex.type = CURVE_SAMPLE_POINT;
			
			
			sample_indices[ii][jj] = V.add_vertex(temp_vertex);
			
			
			Wnew.reset();
			add_mp(target_projection_value,target_projection_value,interval_width); // increment this thingy
			
			
		}
		
		

		
	}  // re: ii (for each edge)
	
	
	
	
	clear_mp(temp); clear_mp(temp1);
	clear_vec_mp(target_projection);
	
	clear_mat_mp(randomizer_matrix);
    
}





int  curve_decomposition::adaptive_set_initial_sample_data()
{

	
	num_samples_each_edge.resize(num_edges);
	
	for (int ii=0; ii<num_edges; ii++){
		num_samples_each_edge[ii]=3; // left, right, mid
	}
	
	
	sample_indices.resize(num_edges);
	
	for (int ii=0; ii<num_edges; ii++)
	{
		sample_indices[ii].resize(3);
		
		sample_indices[ii][0] = edges[ii].left;
		sample_indices[ii][1] = edges[ii].midpt;
		sample_indices[ii][2] = edges[ii].right;
		
	}
	
	return 0;
}


int  curve_decomposition::fixed_set_initial_sample_data(int target_num_samples)
{
	
	
	num_samples_each_edge.resize(num_edges);
	
	for (int ii=0; ii<num_edges; ii++){
		if (edges[ii].is_degenerate()) {
			num_samples_each_edge[ii]=1;
		}
		else{
			num_samples_each_edge[ii]=target_num_samples; // left, right, mid
		}
		
	}
	
	
	sample_indices.resize(num_edges);
	
	for (int ii=0; ii<num_edges; ii++)
	{
		sample_indices[ii].resize(num_samples_each_edge[ii]);
		if (num_samples_each_edge[ii]==1) {
			sample_indices[ii][0] = edges[ii].midpt;
		}
		else
		{
			sample_indices[ii][0] = edges[ii].left;
			
			sample_indices[ii][target_num_samples-1] = edges[ii].right;
		}
		
		
	}
	
	return 0;
}





void curve_decomposition::adaptive_set_initial_refinement_flags(int & num_refinements, std::vector<bool> & refine_flags, std::vector<int> & current_indices,
                                  vertex_set &V,
                                  int current_edge, sampler_configuration & sampler_options)
{
	
	num_refinements = 0; // reset to 0
	refine_flags.resize(num_samples_each_edge[current_edge]-1);

	current_indices.resize(num_samples_each_edge[current_edge]);

	mpf_t dist_away;  mpf_init(dist_away);
	
	vec_mp temp1, temp2;  init_vec_mp(temp1,num_variables-1); init_vec_mp(temp2,num_variables-1);
	temp1->size = temp2->size = num_variables-1;
	
	current_indices[0] = sample_indices[current_edge][0];
	for (int ii=0; ii<(num_samples_each_edge[current_edge]-1); ii++) {
		refine_flags[ii] = false;
		
		
		current_indices[ii+1] = sample_indices[current_edge][ii+1];
		
		for (int jj=0; jj<num_variables-1; jj++) {
			div_mp(&temp1->coord[jj],
                   &V.vertices[sample_indices[current_edge][ii]].pt_mp->coord[jj+1],
                   &V.vertices[sample_indices[current_edge][ii]].pt_mp->coord[0]);
			
			div_mp(&temp2->coord[jj],
                   &V.vertices[sample_indices[current_edge][ii+1]].pt_mp->coord[jj+1],
                   &V.vertices[sample_indices[current_edge][ii+1]].pt_mp->coord[0]);
		}
		
		norm_of_difference(dist_away, temp1, temp2); // get the distance between the two adjacent points.
		
		if ( mpf_cmp(dist_away, sampler_options.TOL)>0 ){
			refine_flags[ii] = true;
			num_refinements++;
		}
	}
    //	printf("done setting refinement flags\n");
	
	clear_vec_mp(temp1); clear_vec_mp(temp2);
	
	mpf_clear(dist_away);
}





//dehomogenizes, takes the average, computes the projection.
//takes in the full projection \pi, including the homogenizing coordinate.
void estimate_new_projection_value(comp_mp result, vec_mp left, vec_mp right, vec_mp pi){
	int ii;
	
	if (left->size != right->size) {
		printf("attempting to estimate_new_projection_value on vectors of different size\n");
		deliberate_segfault();
	}
	
    //	print_point_to_screen_matlab(left,"left");
    //	print_point_to_screen_matlab(right,"right");
    //	print_point_to_screen_matlab(pi,"pi");
	
	vec_mp dehom_left, dehom_right;
	init_vec_mp(dehom_left,left->size-1);   dehom_left->size  = left->size-1;
	init_vec_mp(dehom_right,right->size-1); dehom_right->size = right->size-1;
	
	dehomogenize(&dehom_left,left,pi->size);
	dehomogenize(&dehom_right,right,pi->size);
	
	
	comp_mp temp, temp2, half; init_mp(temp); init_mp(temp2);  init_mp(half);
	
	mpf_set_d(half->r, 0.5); mpf_set_d(half->i, 0.0);
	
    
	set_zero_mp(result);                                           // result = 0;  initialize
	
	for (ii = 0; ii<pi->size-1; ii++) {
		add_mp(temp,&dehom_left->coord[ii],&dehom_right->coord[ii]); //  a = (x+y)
		mul_mp(temp2, temp, half);                                   //  b = a/2
		mul_mp(temp,&pi->coord[ii+1],temp2);                          //  a = b.pi
		set_mp(temp2,result);                                        //  b = result
		add_mp(result, temp, temp2);                                  //  result = a+b
	}
    
	
	// in other words, result += (x+y)/2 \cdot pi
    
	mpf_t zerothresh; mpf_init(zerothresh);
	mpf_set_d(zerothresh, 1e-9);
	if (mpf_cmp(result->i, zerothresh) < 0){
		mpf_set_d(result->i, 0.0);
	}
	
	clear_mp(temp); clear_mp(temp2); clear_mp(half);
	mpf_clear(zerothresh);
	clear_vec_mp(dehom_right);clear_vec_mp(dehom_left);
	
	return;
}




void  curve_decomposition::output_sampling_data(boost::filesystem::path base_path)
{

	boost::filesystem::path samplingName = base_path / "samp.curvesamp";
	
	std::cout << "wrote curve sampling to " << samplingName << std::endl;
	FILE *OUT = safe_fopen_write(samplingName);
	// output the number of vertices
	fprintf(OUT,"%d\n\n",num_edges);
	for (int ii=0; ii<num_edges; ii++) {
		fprintf(OUT,"%d\n",num_samples_each_edge[ii]);
		for (int jj=0; jj<num_samples_each_edge[ii]; jj++) {
			fprintf(OUT,"%d ",sample_indices[ii][jj]);
		}
		fprintf(OUT,"\n\n");
	}
	
	fclose(OUT);
}
















void surface_decomposition::fixed_sampler(vertex_set & V,
										sampler_configuration & sampler_options,
										solver_configuration & solve_options)
{
	int target_num_samples = sampler_options.target_num_samples; // make this dynamic
	
	
	midpoint_config md_config;
	md_config.setup(*this, solve_options); // yep, pass 'this' object into another call. brilliant.
	
	std::cout << "have the slp's in memory for connect the dots" << std::endl;
	
	
	
	std::cout << "critical curve" << std::endl;
	crit_curve.fixed_sampler(V,sampler_options,solve_options,target_num_samples);
	
	std::cout << "sphere curve" << std::endl;
	sphere_curve.fixed_sampler(V,sampler_options,solve_options,target_num_samples);
	
	
	std::cout << "mid slices" << std::endl;
	for (int ii=0; ii<mid_slices.size(); ii++) {
		mid_slices[ii].fixed_sampler(V,sampler_options,solve_options,target_num_samples);
	}
	
	std::cout << "critical slices" << std::endl;
	for (int ii=0; ii<crit_slices.size(); ii++) {
		crit_slices[ii].fixed_sampler(V,sampler_options,solve_options,target_num_samples);
	}
	
//	std::cout << "singular curves" << std::endl;
//	for (int ii=0; ii<singular_curves.size(); ii++) {
//		singular_curves[ii].fixed_sampler(V,sampler_options,solve_options,target_num_samples);
//	}

	witness_set W_midtrack;
	vec_mp blank_point;  init_vec_mp2(blank_point, 0,1024);
	W_midtrack.add_point(blank_point);
	clear_vec_mp(blank_point);
	
	
	comp_mp interval_width; init_mp2(interval_width,1024); set_one_mp(interval_width);
	comp_mp num_intervals;  init_mp2(num_intervals,1024); set_zero_mp(num_intervals);
	
	mpf_set_d(num_intervals->r,double(target_num_samples-1));
	div_mp(interval_width,interval_width,num_intervals);
	
	
	
	V.set_curr_projection(this->pi[0]);
	V.set_curr_input(this->input_filename);

	//once you have the fixed samples of the curves, down here is just making the integer triangles.
	for (int ii=0; ii<num_faces; ii++) {
		
		
		std::cout << "face " << ii << std::endl;
		std::cout << faces[ii];
		
		
		
		
		if (faces[ii].crit_slice_index==-1) {
			continue;
		}
		
		
		int slice_ind = faces[ii].crit_slice_index;
		curve_decomposition * top_, *bottom_;
		
		
		// get the system types
		md_config.system_type_top = faces[ii].system_type_top;
		md_config.system_type_bottom = faces[ii].system_type_bottom;
		
		int num_bottom_vars, num_top_vars;
		if (md_config.system_type_bottom == SYSTEM_CRIT) {
			num_bottom_vars = md_config.num_crit_vars;
		}
		else { //if (md_config.system_type_bottom == SYSTEM_SPHERE)
			num_bottom_vars = md_config.num_sphere_vars;
		}
//		else{
//			
//		}
		
		
		if (md_config.system_type_top == SYSTEM_CRIT) {
			num_top_vars = md_config.num_crit_vars;
		}
		else { //if (md_config.system_type_top == SYSTEM_SPHERE)
			num_top_vars = md_config.num_sphere_vars;
		}
//		else{
//			
//		}
		
		
		
		
		//copy in the start point as three points concatenated.
		
		W_midtrack.num_variables = this->num_variables + num_bottom_vars + num_top_vars;
		W_midtrack.num_synth_vars = W_midtrack.num_variables - this->num_variables;
		change_size_vec_mp(W_midtrack.pts_mp[0], W_midtrack.num_variables); W_midtrack.pts_mp[0]->size = W_midtrack.num_variables; // destructive resize
		
		
		// mid
		int var_counter = 0;
		for (int kk=0; kk<this->num_variables; kk++) {
			set_mp(&W_midtrack.pts_mp[0]->coord[kk], &V.vertices[faces[ii].midpt].pt_mp->coord[kk]);
			var_counter++;
		}
		
		int mid_edge = mid_slices[slice_ind].edge_w_midpt(faces[ii].midpt);
		// bottom
		int offset = var_counter;
		for (int kk=0; kk<num_bottom_vars; kk++) {
			set_mp(&W_midtrack.pts_mp[0]->coord[kk+offset], &V.vertices[mid_slices[slice_ind].edges[mid_edge].left].pt_mp->coord[kk]); // y0
			var_counter++;
		}
		
		// top
		offset = var_counter;
		for (int kk=0; kk<num_top_vars; kk++) {
			set_mp(&W_midtrack.pts_mp[0]->coord[kk+offset], &V.vertices[mid_slices[slice_ind].edges[mid_edge].right].pt_mp->coord[kk]); // y2
			var_counter++;
		}
		
		
		
		
		
		
		
		//copy in the patches appropriate for the systems we will be tracking on.  this could be improved.
		W_midtrack.reset_patches();
		
		for (int qq = 0; qq<this->num_patches; qq++) {
			W_midtrack.add_patch(this->patch[qq]);
		}
		
		if (md_config.system_type_bottom == SYSTEM_CRIT) {
			for (int qq = 0; qq<crit_curve.num_patches; qq++) {
				W_midtrack.add_patch(crit_curve.patch[qq]);
			}
			
			bottom_ = &crit_curve;
		}
		else{
			for (int qq = 0; qq<sphere_curve.num_patches; qq++) {
				W_midtrack.add_patch(sphere_curve.patch[qq]);
			}
			bottom_ = &sphere_curve;
		}
		
		if (md_config.system_type_top == SYSTEM_CRIT) {
			for (int qq = 0; qq<crit_curve.num_patches; qq++) {
				W_midtrack.add_patch(crit_curve.patch[qq]);
			}
			top_ = &crit_curve;
		}
		else{
			for (int qq = 0; qq<sphere_curve.num_patches; qq++) {
				W_midtrack.add_patch(sphere_curve.patch[qq]);
			}
			top_ = &sphere_curve;
		}
		
		
		
		
		
		
		// make u, v target values.
		
		set_mp(md_config.crit_val_left,   &V.vertices[ crit_slices[slice_ind].edges[0].midpt ].projection_values->coord[0]);
		set_mp(md_config.crit_val_right,  &V.vertices[ crit_slices[slice_ind+1].edges[0].midpt ].projection_values->coord[0]);
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		//we need to sample the ribs
		std::vector< std::vector<int> > rib_indices;
		rib_indices.resize(target_num_samples);
		
		for (int jj=0; jj<faces[ii].num_left; jj++) {
			int asdf = faces[ii].left[jj];
//			std::cout << "crit_slices["<<slice_ind<<"].sample_indices["<<asdf<<"].size() = " << crit_slices[slice_ind].sample_indices[asdf].size()<<std::endl;
			
			
			for (int kk = 0; kk< crit_slices[slice_ind].sample_indices[asdf].size(); kk++) {
				rib_indices[0].push_back(crit_slices[slice_ind].sample_indices[asdf][kk]);
			}
		}
			
		for (int jj=0; jj<faces[ii].num_right; jj++) {
			int asdf = faces[ii].right[jj];
			for (int kk = 0; kk< crit_slices[slice_ind+1].sample_indices[asdf].size(); kk++) {
				rib_indices[target_num_samples-1].push_back(crit_slices[slice_ind+1].sample_indices[asdf][kk]);
			}
		}
		
		
		for (int jj=0; jj<rib_indices[0].size(); jj++) {
			std::cout << rib_indices[0][jj] << " ";
		}
		std::cout << std::endl;
		for (int jj=0; jj<rib_indices[target_num_samples-1].size(); jj++) {
			std::cout << rib_indices[target_num_samples-1][jj] << " ";
		}
		std::cout << std::endl;
		
		//check the two rows.
		
		for (int zz=0; zz<int(rib_indices[0].size())-1; zz++) {
			if (mpf_cmp(V.vertices[rib_indices[0][zz]].projection_values->coord[1].r, V.vertices[rib_indices[0][zz+1]].projection_values->coord[1].r) > 0) {
				std::cout << "out of order, cuz these are off:" << std::endl;
				print_comp_matlab(V.vertices[rib_indices[0][zz]].projection_values->coord,"l");
				print_comp_matlab(V.vertices[rib_indices[0][zz+1]].projection_values->coord,"r");
			}
		}
		
		for (int zz=0; zz<int(rib_indices[target_num_samples-1].size())-1; zz++) {
			if (mpf_cmp(V.vertices[rib_indices[target_num_samples-1][zz]].projection_values->coord[1].r, V.vertices[rib_indices[target_num_samples-1][zz+1]].projection_values->coord[1].r) > 0) {
				std::cout << "out of order, cuz these are off:" << std::endl;
				print_comp_matlab(V.vertices[rib_indices[target_num_samples-1][zz]].projection_values->coord,"l");
				print_comp_matlab(V.vertices[rib_indices[target_num_samples-1][zz+1]].projection_values->coord,"r");
			}
		}
		

		
		vertex temp_vertex;
		
		
		
		
		//this is here to get ready to use a single midtrack, followed by many multilins.
//		//get ready to use the multilin tracker.  
//		parse_input_file(this->input_filename); // restores all the temp files generated by the parser, to this folder.
//		solve_options.get_PPD();
		
		//initialize to 0
		set_zero_mp(md_config.u_target);
		
		for (int jj=1; jj<target_num_samples-1; jj++) {
			
			add_mp(md_config.u_target,md_config.u_target,interval_width);
			set_zero_mp(md_config.v_target);
//			print_comp_matlab(md_config.u_target,"u_target");

			rib_indices[jj].resize(target_num_samples);
			
			

			rib_indices[jj][0] = bottom_->sample_indices[faces[ii].bottom][jj];
			rib_indices[jj][target_num_samples-1] = top_->sample_indices[faces[ii].top][jj];
			
			
			for (int kk=1; kk<target_num_samples-1; kk++) {
				add_mp(md_config.v_target,md_config.v_target,interval_width);
				
//				print_comp_matlab(md_config.v_target,"v_target");
				
				
				witness_set W_new;
				midpoint_solver_master_entry_point(W_midtrack, // carries with it the start points, and the linears.
												   &W_new, // new data goes in here
												   md_config,
												   solve_options);
				
				if (W_new.num_pts==0) {
					std::cout << color::red() << "midpoint tracker did not return any points :(" << color::console_default() << std::endl;
					rib_indices[jj][kk] = -1;
					// do something other than continue here.  this is terrible.
					continue;
				}
				

				vec_cp_mp(temp_vertex.pt_mp,W_new.pts_mp[0]);
				temp_vertex.type = SURFACE_SAMPLE_POINT;
				
				
				rib_indices[jj][kk] = V.add_vertex(temp_vertex);
				
				
			}
			
			
			
			
		}
		

		
		//stitch together the rib_indices
		samples.push_back(stitch_triangulation(rib_indices,V));
		

	}
	
	
	
	return;
}



std::vector< triangle > surface_decomposition::stitch_triangulation(const std::vector< std::vector< int > > & rib_indices,
																	const vertex_set & V)
{
#ifdef functionentry_output
	std::cout << "surface_decomposition::stitch_triangulation" << std::endl;
#endif
	
	std::vector< triangle > current_samples;
	
	triangle add_me;
	


	
	
	for (int ii = 0; ii<int(rib_indices.size()-1); ii++) {
		triangulate_two_ribs(rib_indices[ii], rib_indices[ii+1], V, current_samples);
	}
	
	
		
	return current_samples;
}


void triangulate_two_ribs(const std::vector< int > & rib1, const std::vector< int > & rib2,
						  const vertex_set & V,
						  std::vector< triangle> & current_samples)
{
	
	triangle add_me;
	
	if (rib1.size()==1 && rib2.size()==1) {
		return;
	}
	else if (rib1.size()==1) {
		add_me.v1 = rib1[0];
		for (int jj = 0; jj<int(rib2.size()-1); jj++) {
			add_me.v2 = rib2[jj];
			add_me.v3 = rib2[jj+1];
			
			current_samples.push_back(add_me);
		}
	}
	else if (rib2.size()==1) {
		add_me.v1 = rib2[0];
		for (int jj = 0; jj<int(rib1.size()-1); jj++) {
			add_me.v2 = rib1[jj];
			add_me.v3 = rib1[jj+1];
			
			current_samples.push_back(add_me);
		}
	}
	else if (rib1.size()==rib2.size())
	{
		
		for (int jj = 0; jj<int(rib1.size()-1); jj++) {
			add_me.v1 = rib1[jj];
			add_me.v2 = rib2[jj];
			add_me.v3 = rib2[jj+1];
			
			current_samples.push_back(add_me);
			
			add_me.v1 = rib1[jj];
			add_me.v2 = rib1[jj+1];
			add_me.v3 = rib2[jj+1];
			
			current_samples.push_back(add_me);
			
		}
		
	}
	else{
				
		triangulate_two_ribs_by_binning(rib1, rib2,
										V,
										current_samples);
		
		
	}
}


void triangulate_two_ribs_by_binning(const std::vector< int > & rib1, const std::vector< int > & rib2,
									 const vertex_set & V,
									 std::vector< triangle> & current_samples)
{
#ifdef functionentry_output
	std::cout << "triangulate_by_binning" << std::endl;
#endif
	
	
	
	const std::vector<int> * rib_w_more_entries, *rib_w_less_entries;
	
	if (rib1.size() > rib2.size()) {
		rib_w_more_entries = & rib1;
		rib_w_less_entries = & rib2;
	}
	else{
		rib_w_more_entries = & rib2;
		rib_w_less_entries = & rib1;
	}
	
	int nbins = rib_w_less_entries->size();
	
	
	vec_mp bins; init_vec_mp(bins, nbins);
	bins->size = nbins;
	
	comp_mp interval_width, temp; init_mp(interval_width);  init_mp(temp);

	sub_mp(interval_width,
		   &V.vertices[rib_w_less_entries->at(nbins-1)].projection_values->coord[1],
		   &V.vertices[rib_w_less_entries->at(0)].projection_values->coord[1]);
	
	
	set_zero_mp(&bins->coord[0]);
	set_one_mp(&bins->coord[nbins-1]);
	
	for (int ii=1; ii<nbins-1; ii++) {
		sub_mp(temp,
			   &V.vertices[rib_w_less_entries->at(ii)].projection_values->coord[1],
			   &V.vertices[rib_w_less_entries->at(0)].projection_values->coord[1]);
		div_mp(&bins->coord[ii], temp, interval_width);
	}
	
	//ok, have the bins.  now to actually bin the larger of the two ribs.
	
	
	sub_mp(interval_width,
		   &V.vertices[rib_w_more_entries->back()].projection_values->coord[1],
		   &V.vertices[rib_w_more_entries->at(0)].projection_values->coord[1]);
	
	
	//both ribs are guaranteed (by hypothesis) to have increasing projection values in pi[1].
	int curr_bin_ind = 1; // start at the lowest.
	
	for (int ii=0; ii<rib_w_more_entries->size()-1; ii++) {
		
		if (ii==rib_w_more_entries->size()-1) {
			set_one_mp(temp);
		}
		else{
			sub_mp(temp,
				   &V.vertices[rib_w_more_entries->at(ii+1)].projection_values->coord[1],
				   &V.vertices[rib_w_more_entries->at(0)].projection_values->coord[1]);
			div_mp(temp, temp, interval_width);
		}
		
		
		if (mpf_cmp(temp->r, bins->coord[curr_bin_ind].r)>=0) {
			triangle T(rib_w_less_entries->at(curr_bin_ind-1), rib_w_less_entries->at(curr_bin_ind), rib_w_more_entries->at(ii)); // this call could be contracted with the next.
			current_samples.push_back(T);
			
			curr_bin_ind++;
		}

		triangle T(rib_w_less_entries->at(curr_bin_ind-1), rib_w_more_entries->at(ii), rib_w_more_entries->at(ii+1)); // this call could be contracted with the next.
		current_samples.push_back(T);
		
		
	}
	
	
	clear_vec_mp(bins);
	clear_mp(interval_width);
	return;
}

void  surface_decomposition::output_sampling_data(boost::filesystem::path base_path)
{
	
	boost::filesystem::path samplingName = base_path / "samp.surfsamp";
	
	std::cout << "writing surface sampling to " << samplingName << std::endl;
	
	std::ofstream fout(samplingName.string());
	fout << samples.size() << std::endl << std::endl;
	for (int ii=0; ii<int(samples.size()); ii++) {
		
		fout << samples[ii].size() << std::endl;
		
		for (int jj=0; jj<int(samples[ii].size()); jj++) {
			fout << samples[ii][jj] << std::endl;
		}
		fout << std::endl;
	}
	
	fout << std::endl;
	fout.close();
	
	
	
	crit_curve.output_sampling_data(base_path / "curve_crit");
	

	sphere_curve.output_sampling_data(base_path / "curve_sphere");
	
	
	boost::filesystem::path curve_location = base_path / "curve";
	
	std::stringstream converter;
	
	for (int ii=0; ii<mid_slices.size(); ii++) {
		boost::filesystem::path specific_loc = curve_location;
		
		converter << ii;
		specific_loc += "_midslice_";
		specific_loc += converter.str();
		converter.clear(); converter.str("");
		
		
		mid_slices[ii].output_sampling_data(specific_loc);
	}
	
	for (int ii=0; ii<crit_slices.size(); ii++) {
		boost::filesystem::path specific_loc = curve_location;
		
		converter << ii;
		specific_loc += "_critslice_";
		specific_loc += converter.str();
		converter.clear(); converter.str("");
		
		crit_slices[ii].output_sampling_data(specific_loc);
	}
	
	
	
	
}










void set_witness_set_mp(witness_set *W, vec_mp new_linear,vec_mp pts,int num_vars)
{
	
	W->num_variables = num_vars;
	
	W->num_pts=1;
	W->pts_mp=(point_mp *)br_malloc(sizeof(point_mp)); // apparently you can only pass in a single point to copy in.
	
	//initialize the memory
	init_point_mp(W->pts_mp[0],num_vars); W->pts_mp[0]->size = num_vars;
	point_cp_mp(W->pts_mp[0],pts);
	
	
	W->num_linears = 1;
	W->L_mp = (vec_mp *)br_malloc(sizeof(vec_mp));
	init_vec_mp(W->L_mp[0],num_vars); W->L_mp[0]->size = num_vars;
	vec_cp_mp(W->L_mp[0],new_linear);
	
}








int get_dir_mptype_dimen(boost::filesystem::path & Dir_Name, int & MPType, int & dimension){
    
	std::string tempstr;
	std::ifstream fin("Dir_Name");
	fin >> tempstr;
	fin >> MPType;
	fin >> dimension;
	
	Dir_Name = tempstr;
	return MPType;
}




















