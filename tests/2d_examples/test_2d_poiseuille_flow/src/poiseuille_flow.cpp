/**
 * @file 	poiseuille_flow.cpp
 * @brief 	2D poiseuille flow example
 * @details This is the one of the basic test cases for validating viscous flow.
 * 			//TODO: this case is too causal now, it should be revised to validate low-Reynolds number flow (Re = 10?).
 * @author 	Chi Zhang and Xiangyu Hu
 */
/**
  * @brief 	SPHinXsys Library.
  */
#include "sphinxsys.h"
/**
 * @brief Namespace cite here.
 */
using namespace SPH;
/**
 * @brief Basic geometry parameters and numerical setup.
 */
Real DL = 1.0e-3;				 /**< Tank length. */
Real DH = 1.0e-3;				 /**< Tank height. */
Real resolution_ref = DH / 20.0; /**< Initial reference particle spacing. */
Real BW = resolution_ref * 4;	 /**< Extending width for BCs. */
/** Domain bounds of the system. */
BoundingBox system_domain_bounds(Vec2d(-BW, -BW), Vec2d(DL + BW, DH + BW));
/**
 * @brief Material properties of the fluid.
 */
Real rho0_f = 1000.0;				   /**< Reference density of fluid. */
Real gravity_g = 1.0e-4;			   /**< Gravity force of fluid. */
Real mu_f = 1.0e-6;					   /**< Viscosity. */
Real U_f = gravity_g * DH * DH / mu_f; /**< Characteristic velocity. */
Real c_f = 10.0 * U_f;				   /**< Reference sound speed. */
/**
 * @brief 	Fluid body definition.
 */
class WaterBlock : public MultiPolygonShape
{
public:
	explicit WaterBlock(const std::string &shape_name) : MultiPolygonShape(shape_name)
	{
		/** Geometry definition. */
		std::vector<Vecd> water_block_shape;
		water_block_shape.push_back(Vecd(0.0, 0.0));
		water_block_shape.push_back(Vecd(0.0, DH));
		water_block_shape.push_back(Vecd(DL, DH));
		water_block_shape.push_back(Vecd(DL, 0.0));
		water_block_shape.push_back(Vecd(0.0, 0.0));
		multi_polygon_.addAPolygon(water_block_shape, ShapeBooleanOps::add);
	}
};
/**
 * @brief 	Wall boundary body definition.
 */
class WallBoundary : public MultiPolygonShape
{
public:
	explicit WallBoundary(const std::string &shape_name) : MultiPolygonShape(shape_name)
	{
		/** Geometry definition. */
		std::vector<Vecd> outer_wall_shape;
		outer_wall_shape.push_back(Vecd(-BW, -BW));
		outer_wall_shape.push_back(Vecd(-BW, DH + BW));
		outer_wall_shape.push_back(Vecd(DL + BW, DH + BW));
		outer_wall_shape.push_back(Vecd(DL + BW, -BW));
		outer_wall_shape.push_back(Vecd(-BW, -BW));
		std::vector<Vecd> inner_wall_shape;
		inner_wall_shape.push_back(Vecd(-2.0 * BW, 0.0));
		inner_wall_shape.push_back(Vecd(-2.0 * BW, DH));
		inner_wall_shape.push_back(Vecd(DL + 2.0 * BW, DH));
		inner_wall_shape.push_back(Vecd(DL + 2.0 * BW, 0.0));
		inner_wall_shape.push_back(Vecd(-2.0 * BW, 0.0));

		multi_polygon_.addAPolygon(outer_wall_shape, ShapeBooleanOps::add);
		multi_polygon_.addAPolygon(inner_wall_shape, ShapeBooleanOps::sub);
	}
};
/**
 * @brief 	Main program starts here.
 */
int main()
{
	/**
	 * @brief Build up -- a SPHSystem --
	 */
	SPHSystem system(system_domain_bounds, resolution_ref);
	/** Set the starting time. */
	GlobalStaticVariables::physical_time_ = 0.0;
	/** Tag for computation from restart files. 0: not from restart files. */
	system.restart_step_ = 0;
	/**
	 * @brief Material property, particles and body creation of fluid.
	 */
	FluidBody water_block(system, makeShared<WaterBlock>("WaterBody"));
	water_block.defineParticlesAndMaterial<FluidParticles, WeaklyCompressibleFluid>(rho0_f, c_f, mu_f);
	water_block.generateParticles<ParticleGeneratorLattice>();
	/**
	 * @brief 	Particle and body creation of wall boundary.
	 */
	SolidBody wall_boundary(system, makeShared<WallBoundary>("Wall"));
	wall_boundary.defineParticlesAndMaterial<SolidParticles, Solid>();
	wall_boundary.generateParticles<ParticleGeneratorLattice>();
	/** topology */
	ComplexBodyRelation water_block_complex(water_block, {&wall_boundary});
	/**
	 * @brief 	Define all numerical methods which are used in this case.
	 */
	/**
	 * @brief 	Methods used for time stepping.
	 */
	/** Define external force. */
	SimpleDynamics<NormalDirectionFromBodyShape> wall_boundary_normal_direction(wall_boundary);
	/** Initialize particle acceleration. */
	SimpleDynamics<TimeStepInitialization> initialize_a_fluid_step(water_block, makeShared<Gravity>(Vecd(gravity_g, 0.0)));
	/** Periodic BCs in x direction. */
	PeriodicConditionUsingCellLinkedList periodic_condition(water_block, water_block.getBodyShapeBounds(), xAxis);
	/**
	 * @brief 	Algorithms of fluid dynamics.
	 */
	/** Evaluation of density by summation approach. */
	fluid_dynamics::DensitySummationComplex update_density_by_summation(water_block_complex);
	/** Time step size without considering sound wave speed. */
	ReduceDynamics<fluid_dynamics::AdvectionTimeStepSize> get_fluid_advection_time_step_size(water_block, U_f);
	/** Time step size with considering sound wave speed. */
	ReduceDynamics<fluid_dynamics::AcousticTimeStepSize> get_fluid_time_step_size(water_block);
	/** Pressure relaxation algorithm without Riemann solver for viscous flows. */
	fluid_dynamics::PressureRelaxationWithWall pressure_relaxation(water_block_complex);
	/** Pressure relaxation algorithm by using position verlet time stepping. */
	fluid_dynamics::DensityRelaxationRiemannWithWall density_relaxation(water_block_complex);
	/** Computing viscous acceleration. */
	InteractionDynamics<fluid_dynamics::ViscousAccelerationWithWall> viscous_acceleration(water_block_complex);
	/** Impose transport velocity. */
	InteractionDynamics<fluid_dynamics::TransportVelocityCorrectionComplex> transport_velocity_correction(water_block_complex);
	/**
	 * @brief Output.
	 */
	IOEnvironment io_environment(system);
	/** Output the body states. */
	BodyStatesRecordingToVtp body_states_recording(io_environment, system.real_bodies_);
	/** Output the body states for restart simulation. */
	RestartIO restart_io(io_environment, system.real_bodies_);
	/**
	 * @brief Setup geometry and initial conditions.
	 */
	system.initializeSystemCellLinkedLists();
	periodic_condition.update_cell_linked_list_.parallel_exec();
	system.initializeSystemConfigurations();
	wall_boundary_normal_direction.parallel_exec();
	/**
	 * @brief The time stepping starts here.
	 */
	/** If the starting time is not zero, please setup the restart time step ro read in restart states. */
	if (system.restart_step_ != 0)
	{
		GlobalStaticVariables::physical_time_ = restart_io.readRestartFiles(system.restart_step_);
		water_block.updateCellLinkedList();
		periodic_condition.update_cell_linked_list_.parallel_exec();
		water_block_complex.updateConfiguration();
	}
	/** Output the start states of bodies. */
	body_states_recording.writeToFile(0);
	/**
	 * @brief 	Basic parameters.
	 */
	size_t number_of_iterations = system.restart_step_;
	int screen_output_interval = 100;
	int restart_output_interval = screen_output_interval * 10;
	Real end_time = 20.0;	/**< End time. */
	Real Output_Time = 0.1; /**< Time stamps for output of body states. */
	Real dt = 0.0;			/**< Default acoustic time step sizes. */
	/** statistics for computing CPU time. */
	tick_count t1 = tick_count::now();
	tick_count::interval_t interval;
	tick_count::interval_t interval_computing_time_step;
	tick_count::interval_t interval_computing_pressure_relaxation;
	tick_count::interval_t interval_updating_configuration;
	tick_count time_instance;
	/**
	 * @brief 	Main loop starts here.
	 */
	while (GlobalStaticVariables::physical_time_ < end_time)
	{
		Real integration_time = 0.0;
		/** Integrate time (loop) until the next output time. */
		while (integration_time < Output_Time)
		{
			/** Acceleration due to viscous force and gravity. */
			time_instance = tick_count::now();
			initialize_a_fluid_step.parallel_exec();
			Real Dt = get_fluid_advection_time_step_size.parallel_exec();
			update_density_by_summation.parallel_exec();
			//viscous_acceleration.parallel_exec();
			transport_velocity_correction.parallel_exec(Dt);
			interval_computing_time_step += tick_count::now() - time_instance;
			/** Dynamics including pressure relaxation. */
			time_instance = tick_count::now();
			Real relaxation_time = 0.0;
			while (relaxation_time < Dt)
			{
				dt = SMIN(get_fluid_time_step_size.parallel_exec(), Dt);
				pressure_relaxation.parallel_exec(dt);
				viscous_acceleration.parallel_exec(dt);
				density_relaxation.parallel_exec(dt);
				relaxation_time += dt;
				integration_time += dt;
				GlobalStaticVariables::physical_time_ += dt;
			}
			interval_computing_pressure_relaxation += tick_count::now() - time_instance;
			if (number_of_iterations % screen_output_interval == 0)
			{
				std::cout << std::fixed << std::setprecision(9) << "N=" << number_of_iterations << "	Time = "
						  << GlobalStaticVariables::physical_time_
						  << "	Dt = " << Dt << "	dt = " << dt << "\n";

				if (number_of_iterations % restart_output_interval == 0)
					restart_io.writeToFile(number_of_iterations);
			}
			number_of_iterations++;
			/** Update cell linked list and configuration. */
			time_instance = tick_count::now();
			/** Water block configuration and periodic condition. */
			periodic_condition.bounding_.parallel_exec();
			water_block.updateCellLinkedList();
			periodic_condition.update_cell_linked_list_.parallel_exec();
			water_block_complex.updateConfiguration();
			interval_updating_configuration += tick_count::now() - time_instance;
		}
		tick_count t2 = tick_count::now();
		body_states_recording.writeToFile();
		tick_count t3 = tick_count::now();
		interval += t3 - t2;
	}
	tick_count t4 = tick_count::now();

	tick_count::interval_t tt;
	tt = t4 - t1 - interval;
	std::cout << "Total wall time for computation: " << tt.seconds()
			  << " seconds." << std::endl;
	std::cout << std::fixed << std::setprecision(9) << "interval_computing_time_step ="
			  << interval_computing_time_step.seconds() << "\n";
	std::cout << std::fixed << std::setprecision(9) << "interval_computing_pressure_relaxation = "
			  << interval_computing_pressure_relaxation.seconds() << "\n";
	std::cout << std::fixed << std::setprecision(9) << "interval_updating_configuration = "
			  << interval_updating_configuration.seconds() << "\n";

	return 0;
}
