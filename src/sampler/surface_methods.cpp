#include "sampler.hpp"





void Surface::fixed_sampler(VertexSet & V,
							sampler_configuration & sampler_options,
							SolverConfiguration & solve_options)
{
	
	FixedSampleCurves(V, sampler_options, solve_options);

	//once you have the fixed samples of the curves, down here is just making the integer triangles.
	for (unsigned int ii=0; ii<num_faces(); ii++) {
		
		if (faces_[ii].is_degenerate() || faces_[ii].is_malformed())
			continue;
		
		std::cout << "Face " << ii << std::endl;
		if (sampler_options.verbose_level()>=1)
			std::cout << faces_[ii];

		FixedSampleFace(ii, V, sampler_options, solve_options);
	} // re: for ii, that is for the faces
	
	return;
}


void Surface::FixedSampleCurves(VertexSet & V,
								sampler_configuration & sampler_options,
								SolverConfiguration & solve_options)
{
	int target_num_samples = sampler_options.target_num_samples; 
	
	std::cout << "critical curve" << std::endl;
	crit_curve().fixed_sampler(V,sampler_options,solve_options,target_num_samples);

	std::cout << "sphere curve" << std::endl;
	sphere_curve().fixed_sampler(V,sampler_options,solve_options,target_num_samples);

	std::cout << "mid slices" << std::endl;
	for (auto ii=mid_slices_iter_begin(); ii!=mid_slices_iter_end(); ii++) {
		ii->fixed_sampler(V,sampler_options,solve_options,target_num_samples);
	}
	
	std::cout << "critical slices" << std::endl;
	for (auto ii=crit_slices_iter_begin(); ii!=crit_slices_iter_end(); ii++) {
		ii->adaptive_sampler_distance(V,sampler_options,solve_options);
	}
	
	if (num_singular_curves()>0) {
		std::cout << "singular curves" << std::endl;
		for (auto iter = singular_curves_iter_begin(); iter!= singular_curves_iter_end(); ++iter) {
			iter->second.fixed_sampler(V,sampler_options,solve_options,target_num_samples);
		}
	}
}


void Surface::FixedSampleFace(int face_index, VertexSet & V, sampler_configuration & sampler_options,
										SolverConfiguration & solve_options)
{
	int target_num_samples = sampler_options.target_num_samples; 

	const Face& curr_face = faces_[face_index];
	
	V.set_curr_projection(pi(0));
	V.set_curr_input(this->input_filename());
	
	
	
	// set up a bunch of temporaries

	vec_mp dehom_left, dehom_right; init_vec_mp(dehom_left,0);  dehom_left->size  = 0; init_vec_mp(dehom_right,0); dehom_right->size = 0;
	vec_mp blank_point;  init_vec_mp2(blank_point, 0,1024);
	
	comp_mp interval_width; init_mp2(interval_width,1024); set_one_mp(interval_width);
	comp_mp num_intervals;  init_mp2(num_intervals,1024); set_zero_mp(num_intervals);
	
	mpf_set_d(num_intervals->r,double(target_num_samples-1));
	div_mp(interval_width,interval_width,num_intervals);
	
	mpf_t dist_away; mpf_init(dist_away);
	comp_mp target_projection_value; init_mp2(target_projection_value,1024);
	
	comp_mp temp, temp2; init_mp2(temp,1024); init_mp2(temp2,1024);
	comp_mp half; init_mp(half);
	mpf_set_d(half->r, 0.5); mpf_set_d(half->i, 0.0);
	


	
	auto slice_ind = curr_face.crit_slice_index();
	
	const Curve & current_midslice = mid_slices_[slice_ind];
	const Curve & left_critslice = crit_slices_[slice_ind];
	const Curve & right_critslice = crit_slices_[slice_ind+1];
	
	
	const Curve* bottom = curve_with_name(curr_face.system_name_bottom());
	const Curve* top = curve_with_name(curr_face.system_name_top());;
	auto num_bottom_vars = bottom->num_variables();
	auto num_top_vars = top->num_variables();
	



	//this is here to get ready to use a single midtrack, followed by many multilins.
	//get ready to use the multilin tracker.
	parse_input_file(this->input_filename()); // restores all the temp files generated by the parser, to this folder.
	solve_options.get_PPD();
	
	this->randomizer()->setup(this->num_variables()-this->num_patches()-2, solve_options.PPD.num_funcs);
	MultilinConfiguration ml_config(solve_options, this->randomizer());





	
	vec_mp * target_multilin_linears = (vec_mp *) br_malloc(2*sizeof(vec_mp));
	init_vec_mp2(target_multilin_linears[0],this->num_variables(),1024); target_multilin_linears[0]->size = this->num_variables();
	init_vec_mp2(target_multilin_linears[1],this->num_variables(),1024); target_multilin_linears[1]->size = this->num_variables();
	
	vec_cp_mp(target_multilin_linears[0],pi(0));
	vec_cp_mp(target_multilin_linears[1],pi(1));








	WitnessSet W_multilin;
	W_multilin.set_num_variables(this->num_variables());
	W_multilin.set_num_natural_variables(this->num_variables());
	W_multilin.add_point(blank_point);
	W_multilin.add_linear(pi(0)); W_multilin.add_linear(pi(1));
	W_multilin.add_patch(this->patch(0));








	MidpointConfiguration md_config;
	md_config.setup(*this, solve_options);

	// get the system names
	md_config.system_name_mid = this->input_filename().filename().string();
	md_config.system_name_top = curr_face.system_name_top();
	md_config.system_name_bottom = curr_face.system_name_bottom();
	
	
	// make u, v target values.
	set_mp(md_config.crit_val_left,   &(V[ left_critslice.get_edge(0).midpt() ].projection_values())->coord[0]);
	set_mp(md_config.crit_val_right,  &(V[ right_critslice.get_edge(0).midpt() ].projection_values())->coord[0]);









	
	
	WitnessSet W_midtrack;
	W_midtrack.add_point(blank_point);
	
	//copy in the start point as three points concatenated.
	W_midtrack.set_num_variables(this->num_variables() + num_bottom_vars + num_top_vars);
	W_midtrack.set_num_natural_variables(this->num_variables());
	change_size_vec_mp(W_midtrack.point(0), W_midtrack.num_variables());
	W_midtrack.point(0)->size = W_midtrack.num_variables(); // destructive resize
	
	
	// mid
	int var_counter = 0;
	for (int kk=0; kk<this->num_variables(); kk++) {
		set_mp(&W_midtrack.point(0)->coord[kk], &(V[curr_face.midpt()].point())->coord[kk]);
		var_counter++;
	}
	
	int mid_edge = current_midslice.edge_w_midpt(curr_face.midpt());
	// bottom
	int offset = var_counter;
	for (int kk=0; kk<num_bottom_vars; kk++) {
		set_mp(& W_midtrack.point(0)->coord[kk+offset], &(V[current_midslice.get_edge(mid_edge).left()].point())->coord[kk]); // y0
		var_counter++;
	}
	
	// top
	offset = var_counter;
	for (int kk=0; kk<num_top_vars; kk++) {
		set_mp(& W_midtrack.point(0)->coord[kk+offset], &(V[current_midslice.get_edge(mid_edge).right()].point())->coord[kk]); // y2
		var_counter++;
	}
	
	//copy in the patches appropriate for the systems we will be tracking on.
	W_midtrack.copy_patches(*this);
	W_midtrack.copy_patches(*bottom);
	W_midtrack.copy_patches(*top);
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	//we need to sample the ribs
	std::vector< std::vector<int> > ribs;
	ribs.resize(target_num_samples);
	
	
	
	// populate the ribs for the left and right edges, which were generated prior in curve sampling methods.
	for (unsigned int jj=0; jj<curr_face.num_left(); jj++) {
		int left_edge_index = curr_face.left_edge(jj);
		
		for (unsigned int kk = 0; kk< left_critslice.num_samples_on_edge(left_edge_index); kk++) {
			if (jj>0 && kk==0)
				if (left_critslice.sample_index(left_edge_index,kk)==ribs[0].back())
					continue;

			ribs[0].push_back(left_critslice.sample_index(left_edge_index,kk));
		}
	}
	
	for (unsigned int jj=0; jj<curr_face.num_right(); jj++) {
		int right_edge_index = curr_face.right_edge(jj);
		for (unsigned int kk = 0; kk< right_critslice.num_samples_on_edge(right_edge_index); kk++) {
			if (jj>0 && kk==0)
				if (right_critslice.sample_index(right_edge_index,kk)==ribs[target_num_samples-1].back())
					continue;
			
			ribs[target_num_samples-1].push_back(right_critslice.sample_index(right_edge_index,kk));
		}
	}
	
	
	
	
	if (sampler_options.verbose_level()>=2) {
		std::cout << "left rib, from left curve edges:\n";
		for (unsigned int jj=0; jj<ribs[0].size(); jj++) {
			std::cout << ribs[0][jj] << " ";
		}
		std::cout << std::endl;

		std::cout << "right rib, from right curve edges:\n";
		for (unsigned int jj=0; jj<ribs[target_num_samples-1].size(); jj++) {
			std::cout << ribs[target_num_samples-1][jj] << " ";
		}
		std::cout << std::endl;
	}
	
	
	
	
	Vertex temp_vertex;
	

	
	
	//initialize to 0
	set_zero_mp(md_config.u_target); // start at the left
	
	for (int jj=1; jj<target_num_samples-1; jj++) {
		if (sampler_options.verbose_level()>=1)
			std::cout << "sampling rib " << jj << std::endl;

		add_mp(md_config.u_target,md_config.u_target,interval_width);
		set_mp(md_config.v_target,half); // start on the bottom one
		



		int curr_bottom_index;
		int curr_top_index;
		try {
			curr_bottom_index = bottom->sample_index(curr_face.bottom_edge(),jj);
			curr_top_index = top->sample_index(curr_face.top_edge(),jj);			
		} 
		catch (std::logic_error& e) {
			std::cout << "not completing sampling this face.  reason:" << std::endl << e.what() << std::endl;
			continue;
		}
		
		
		
		
		
		
		SolverOutput fillme;
		int success_indicator = midpoint_solver_master_entry_point(W_midtrack, // carries with it the start points, and the linears.
																   fillme, // new data goes in here
																   md_config,
															   solve_options);

		int startpt_index;
		if (success_indicator!=SUCCESSFUL) {
			std::cout << color::red() << "midpoint solver unsuccesful at generating first point on rib" << color::console_default() << std::endl;
			continue;
		}
		else {
			WitnessSet W_new;
			fillme.get_noninfinite_w_mult_full(W_new);
			if (W_new.num_points()==0) {
				std::cout << color::red() << "midpoint tracker did not return any noninfinite points" << color::console_default() << std::endl;
				continue;
			}
			else{
				
				temp_vertex.set_type(SURFACE_SAMPLE_POINT);
				temp_vertex.set_point(W_new.point(0));
				startpt_index = V.add_vertex(temp_vertex);

				// need to set the values of the projections in the linears -- they are not unit-scaled as is the midpoint tracker.

				// copy in the start point for the multilin method, as the terminal point from the previous call.
				vec_cp_mp(W_multilin.point(0),V[startpt_index].point());
				W_multilin.point(0)->size = this->num_variables();
				neg_mp(&W_multilin.linear(0)->coord[0],&(V[startpt_index].projection_values())->coord[0]);
				neg_mp(&W_multilin.linear(1)->coord[0],&(V[startpt_index].projection_values())->coord[1]);
				
				real_threshold(&W_multilin.linear(0)->coord[0],(V.T())->real_threshold);
				real_threshold(&W_multilin.linear(1)->coord[0],(V.T())->real_threshold);

				neg_mp(&target_multilin_linears[0]->coord[0],&(V[startpt_index].projection_values())->coord[0]);

				real_threshold(&target_multilin_linears[0]->coord[0],(V.T())->real_threshold);								
				// projection value 1 will be set later, when contructing the rib.
			}
		}
		
		
		
		std::vector<int> refined_rib(3);
		refined_rib[0] = curr_bottom_index;
		refined_rib[1] = startpt_index;
		refined_rib[2] = curr_top_index;
		
		
		std::vector<bool> refine_flags(2, true); // guarantee at least 5 points on the rib.  seems reasonable
		bool need_refinement = true;
		unsigned pass_number = 0;
		while ( need_refinement && (pass_number < sampler_options.maximum_num_iterations) )
		{
			assert( (refine_flags.size() == refined_rib.size()-1) && "refinement flags must be one less than num entries on rib");
			
			std::vector<int> temp_rib;
			std::vector<bool> refine_flags_next;
			need_refinement = false; // reset to no, in case don't need.  will set to true if point too far apart
			
			for (unsigned rr=0; rr<refine_flags.size(); ++rr) {
				temp_rib.push_back(refined_rib[rr]);
				if (refine_flags[rr]) {
					estimate_new_projection_value(target_projection_value,		// the new value
												  V[refined_rib[rr]].point(),	//
												  V[refined_rib[rr+1]].point(), // two points input
												  pi(1));

					neg_mp(&target_multilin_linears[1]->coord[0],target_projection_value);
					
					if (sampler_options.verbose_level()>=4)
					{
						std::cout << "refining rib, tracking from\n";
						print_point_to_screen_matlab(W_multilin.point(0),"startpt");

						print_point_to_screen_matlab(W_multilin.linear(0),"start_linear0");
						print_point_to_screen_matlab(W_multilin.linear(1),"start_linear1");

						print_point_to_screen_matlab(target_multilin_linears[0],"target_linear0");
						print_point_to_screen_matlab(target_multilin_linears[1],"target_linear1");

						print_comp_matlab(&V[refined_rib[rr]].projection_values()->coord[1],"left proj val");
						print_comp_matlab(&V[refined_rib[rr+1]].projection_values()->coord[1],"right proj val");
					}

					SolverOutput track_result;
					success_indicator = multilin_solver_master_entry_point(W_multilin,         // WitnessSet
																		   track_result, // the new data is put here!
																		   target_multilin_linears,
																		   ml_config,
																		   solve_options);
					
					WitnessSet W_new;
					track_result.get_noninfinite_w_mult_full(W_new);
					

					if (W_new.num_points()==0) {
						std::cout << color::red() << "multilin tracker did not return any noninfinite points :(" << color::console_default() << std::endl;
						refine_flags_next.push_back(false);
						continue;
					}

					
					dehomogenize(&dehom_right,W_new.point(0));
					if (!checkForReal_mp(dehom_right, (V.T())->real_threshold))
					{
						std::cout << color::red() << "got non-real solution sample... something strange going on!" << color::console_default() << '\n';
						refine_flags_next.push_back(false);
						continue;
						
					}


					temp_vertex.set_point(W_new.point(0));
					temp_vertex.set_type(SURFACE_SAMPLE_POINT);
					temp_rib.push_back(V.add_vertex(temp_vertex));

					dehomogenize(&dehom_left,V[refined_rib[rr]].point());
					norm_of_difference_mindim(dist_away,
									   dehom_left, // the current new point
									   dehom_right);
					
					
					refine_flags_next.push_back(mpf_cmp(dist_away, sampler_options.TOL)>0);
					if (refine_flags_next.back())
						need_refinement = true;
					
					
					dehomogenize(&dehom_left,V[refined_rib[rr+1]].point());
					
					norm_of_difference_mindim(dist_away,
									   dehom_left, // the current new point
									   dehom_right);
					
					
					refine_flags_next.push_back(mpf_cmp(dist_away, sampler_options.TOL)>0);
					if (refine_flags_next.back())
						need_refinement = true;
				} // re: if refine_flags[rr]
				else
					refine_flags_next.push_back(false);
				
			} // re: for (unsigned rr=0
			temp_rib.push_back(refined_rib.back());
			swap(temp_rib,refined_rib);
			swap(refine_flags_next,refine_flags);

			++pass_number;
		} // re: while
		
		swap(ribs[jj], refined_rib);
	}
	
	//check the ribs.
	for (auto& r : ribs)
	{
		for (int zz=0; zz<int(r.size())-1; zz++) {
			if (mpf_cmp((V[r[zz]].projection_values())->coord[1].r, (V[r[zz+1]].projection_values())->coord[1].r) > 0) {
				std::cout << "out of order, cuz these are off:" << std::endl;
				print_comp_matlab((V[r[zz]].projection_values())->coord,"l");
				print_comp_matlab((V[r[zz+1]].projection_values())->coord,"r");
			}
		}
	}

	StitchRibs(ribs,V);

	
	clear_vec_mp(blank_point);
	clear_mp(target_projection_value);
	mpf_clear(dist_away);
	clear_mp(temp); clear_mp(temp2); clear_mp(interval_width); clear_mp(num_intervals);
	
	clear_vec_mp(target_multilin_linears[0]); clear_vec_mp(target_multilin_linears[1]); free(target_multilin_linears);
	
	clear_vec_mp(dehom_right); clear_vec_mp(dehom_left);
}


void Surface::StitchRibs(std::vector<std::vector<int> > const& ribs, VertexSet & V)
{
	std::vector< Triangle > current_samples;
	for (auto r = ribs.begin(); r!=ribs.end()-1; r++) {
		
		if (r->size()==0 || (r+1)->size()==0) {
			std::cout << "empty rib!" << std::endl;
			continue;
		}
		triangulate_two_ribs_by_angle_optimization(*r, *(r+1), V, (V.T())->real_threshold, current_samples);
	}
	
	samples_.push_back(current_samples); 
}




void  Surface::output_sampling_data(boost::filesystem::path base_path) const
{
	
	boost::filesystem::path samplingName = base_path / "samp.surfsamp";
	
	std::cout << "writing surface sampling to " << samplingName << std::endl;
	
	std::ofstream fout(samplingName.string());
	fout << samples_.size() << std::endl << std::endl;
	for (int ii=0; ii<int(samples_.size()); ii++) {
		
		fout << samples_[ii].size() << std::endl;
		
		for (int jj=0; jj<int(samples_[ii].size()); jj++) {
			fout << samples_[ii][jj] << std::endl;
		}
		fout << std::endl;
	}
	
	fout << std::endl;
	fout.close();
	
	
	
	crit_curve_.output_sampling_data(base_path / "curve_crit");
	

	sphere_curve_.output_sampling_data(base_path / "curve_sphere");
	
	
	boost::filesystem::path curve_location = base_path / "curve";
	
	std::stringstream converter;
	
	for (unsigned int ii=0; ii<mid_slices_.size(); ii++) {
		boost::filesystem::path specific_loc = curve_location;
		
		converter << ii;
		specific_loc += "_midslice_";
		specific_loc += converter.str();
		converter.clear(); converter.str("");
		
		
		mid_slices_[ii].output_sampling_data(specific_loc);
	}
	
	for (unsigned int ii=0; ii<crit_slices_.size(); ii++) {
		boost::filesystem::path specific_loc = curve_location;
		
		converter << ii;
		specific_loc += "_critslice_";
		specific_loc += converter.str();
		converter.clear(); converter.str("");
		
		crit_slices_[ii].output_sampling_data(specific_loc);
	}
	
	
	curve_location += "_singular_mult_";
	if (num_singular_curves()>0) {
		
		
		for (auto iter = singular_curves_.begin(); iter!= singular_curves_.end(); ++iter) {
			boost::filesystem::path specific_loc = curve_location;
			converter << iter->first.multiplicity() << "_" << iter->first.index();
			
			specific_loc += converter.str();
			converter.clear(); converter.str("");
			
			iter->second.output_sampling_data(specific_loc);
		}
	}
	
}








