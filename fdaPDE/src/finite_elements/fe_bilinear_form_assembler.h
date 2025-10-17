// This file is part of fdaPDE, a C++ library for physics-informed
// spatial and functional data analysis.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __FDAPDE_FE_BILINEAR_FORM_ASSEMBLER_H__
#define __FDAPDE_FE_BILINEAR_FORM_ASSEMBLER_H__

#include "header_check.h"

namespace fdapde {
namespace internals {
  
// galerkin and petrov-galerkin vector finite element assembly loop
template <typename Triangulation_, typename Form_, int Options_, typename... Quadrature_>
class fe_bilinear_form_assembly_loop :
    public fe_assembler_base<Triangulation_, Form_, Options_, Quadrature_...>,
    public assembly_xpr_base<fe_bilinear_form_assembly_loop<Triangulation_, Form_, Options_, Quadrature_...>> {
   public:
    // detect trial and test spaces from bilinear form
    using TrialSpace = trial_space_t<Form_>;
    using TestSpace  = test_space_t <Form_>;
    static_assert(TrialSpace::local_dim == TestSpace::local_dim && TrialSpace::embed_dim == TestSpace::embed_dim);
    static constexpr bool is_galerkin = std::is_same_v<TrialSpace, TestSpace>;
    static constexpr bool is_petrov_galerkin = !is_galerkin;
    using Base = fe_assembler_base<Triangulation_, Form_, Options_, Quadrature_...>;
    using Form = typename Base::Form;
    using DofHandlerType = typename Base::DofHandlerType;
    using discretization_category = typename TestSpace::discretization_category;
    fdapde_static_assert(
      std::is_same_v<discretization_category FDAPDE_COMMA typename TrialSpace::discretization_category>,
      TEST_AND_TRIAL_SPACE_MUST_HAVE_THE_SAME_DISCRETIZATION_CATEGORY);
    static constexpr int local_dim = Base::local_dim;
    static constexpr int embed_dim = Base::embed_dim;
    using Base::form_;
    // as trial and test spaces could be different, we here need to redefine some properties of Base
    // trial space properties
    using TrialFeType = typename TrialSpace::FeType;
    using trial_fe_traits = std::conditional_t<
      Options_ == CellMajor, fe_cell_assembler_traits<TrialSpace, Quadrature_...>,
      fe_face_assembler_traits<TrialSpace, Quadrature_...>>;
    using trial_dof_descriptor = typename trial_fe_traits::dof_descriptor;
    using TrialBasisType = typename trial_dof_descriptor::BasisType;
    static constexpr int n_trial_basis = TrialBasisType::n_basis;
    static constexpr int n_trial_components = TrialSpace::n_components;
    // test space properties
    using TestFeType = typename TestSpace::FeType;
    using test_fe_traits = std::conditional_t<
      Options_ == CellMajor, fe_cell_assembler_traits<TestSpace, Quadrature_...>,
      fe_face_assembler_traits<TestSpace, Quadrature_...>>;
    using test_dof_descriptor = typename test_fe_traits::dof_descriptor;
    using TestBasisType = typename test_dof_descriptor::BasisType;
    static constexpr int n_test_basis = TestBasisType::n_basis;
    static constexpr int n_test_components = TestSpace::n_components;

    using Quadrature = std::conditional_t<
      (is_galerkin || sizeof...(Quadrature_) > 0), typename Base::Quadrature,
      higher_degree_fe_quadrature_t<
        typename TrialFeType::template cell_quadrature_t<local_dim>,
        typename TestFeType ::template cell_quadrature_t<local_dim>>>;
    static constexpr int n_quadrature_nodes = Quadrature::order;
   private:
    // selected Quadrature could be different than Base::Quadrature, evaluate trial and (re-evaluate) test functions
    static constexpr auto test_shape_values_  = Base::template eval_shape_values<Quadrature, test_fe_traits >();
    static constexpr auto trial_shape_values_ = Base::template eval_shape_values<Quadrature, trial_fe_traits>();
    static constexpr auto test_shape_grads_   = Base::template eval_shape_grads <Quadrature, test_fe_traits >();
    static constexpr auto trial_shape_grads_  = Base::template eval_shape_grads <Quadrature, trial_fe_traits>();
    static constexpr auto test_shape_hess_    = Base::template eval_shape_hess  <Quadrature, test_fe_traits >();
    static constexpr auto trial_shape_hess_   = Base::template eval_shape_hess  <Quadrature, trial_fe_traits>();
    // private data members
    const DofHandlerType* trial_dof_handler_;
    Quadrature quadrature_ {};
    constexpr const DofHandlerType* test_dof_handler() const { return Base::dof_handler_; }
    constexpr const DofHandlerType* trial_dof_handler() const {
        return is_galerkin ? Base::dof_handler_ : trial_dof_handler_;
    }
    const TrialSpace* trial_space_;
   public:
    fe_bilinear_form_assembly_loop() = default;
    fe_bilinear_form_assembly_loop(
      const Form_& form, typename Base::fe_traits::geo_iterator begin, typename Base::fe_traits::geo_iterator end,
      const Quadrature_&... quadrature)
        requires(sizeof...(quadrature) <= 1)
        : Base(form, begin, end, quadrature...), trial_space_(&internals::trial_space(form_)) {
        if constexpr (is_petrov_galerkin) { trial_dof_handler_ = &internals::trial_space(form_).dof_handler(); }
        fdapde_assert(test_dof_handler()->n_dofs() != 0 && trial_dof_handler()->n_dofs() != 0);
    }

    Eigen::SparseMatrix<double> assemble() const {
        Eigen::SparseMatrix<double> assembled_mat(test_dof_handler()->n_dofs(), trial_dof_handler()->n_dofs());
        std::vector<Eigen::Triplet<double>> triplet_list;

        //riserva spazio per evitare riallocamento vettore
        int n_cell = this->Base::dof_handler_->triangulation()->n_cells();
        int triple_per_cella = n_trial_basis * n_test_basis; //9 qui;
        int tot_triple = n_cell * triple_per_cella;
        triplet_list.reserve(tot_triple);
auto start = std::chrono::high_resolution_clock::now();
	assemble(triplet_list);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
std::cout<<duration.count()<<",";
	// linearity of the integral is implicitly used here, as duplicated triplets are summed up (see Eigen docs)
        assembled_mat.setFromTriplets(triplet_list.begin(), triplet_list.end());
        assembled_mat.makeCompressed();

        return assembled_mat;
    }

    void assemble(std::vector<Eigen::Triplet<double>>& triplet_list) const {
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );
        // prepare assembly loop
	Eigen::Matrix<int, Dynamic, 1> test_active_dofs, trial_active_dofs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, embed_dim, n_test_components >> test_grads;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, embed_dim, n_trial_components>> trial_grads;
        Matrix<double, n_test_basis , n_quadrature_nodes> test_divs;
        Matrix<double, n_trial_basis, n_quadrature_nodes> trial_divs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, n_test_components,  embed_dim, embed_dim>>
	  test_hess;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, n_trial_components, embed_dim, embed_dim>>
          trial_hess;

        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
            Base::distribute_quadrature_nodes(begin, end);
        }
        // start assembly loop
        internals::fe_assembler_packet<embed_dim> fe_packet(n_trial_components, n_test_components);
	// if hessians are zero, assemble physical hessian once and never update
        constexpr bool test_hess_is_zero = std::all_of(
          test_shape_hess_.data(), test_shape_hess_.data() + test_shape_hess_.size(), [](double x) { return x == 0; });
        constexpr bool trial_hess_is_zero =
          std::all_of(trial_shape_hess_.data(), trial_shape_hess_.data() + trial_shape_hess_.size(), [](double x) {
              return x == 0;
          });
        if constexpr (test_hess_is_zero ) { std::fill_n(fe_packet.test_hess.data(), fe_packet.test_hess.size(), 0.0); }
        if constexpr (trial_hess_is_zero) {
            std::fill_n(fe_packet.trial_hess.data(), fe_packet.trial_hess.size(), 0.0);
        }

        int local_cell_id = 0;
        for (iterator it = begin; it != end; ++it) {
            // update fe_packet content based on form requests
            fe_packet.measure = it->measure();
            if constexpr (Form::XprBits & int(geo_assembler_flags::compute_geo_id)) { fe_packet.geo_id = it->id(); }
            if constexpr (Form::XprBits & int(geo_assembler_flags::compute_face_normal)) {
                fdapde_static_assert(Options_ == FaceMajor, BILINEAR_FORM_REQUIRES_A_FACE_MAJOR_ASSEMBLY_LOOP);
                fe_packet.normal.assign_inplace_from(it->normal());
            }
            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                Base::eval_shape_grads_on_cell(it, test_shape_grads_, test_grads);
                if constexpr (is_petrov_galerkin) Base::eval_shape_grads_on_cell(it, trial_shape_grads_, trial_grads);
            }
            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                fdapde_static_assert(
                  n_test_components != 1 || n_trial_components != 1,
                  DIVERGENCE_OPERATOR_IS_DEFINED_ONLY_FOR_VECTOR_ELEMENTS);
                if constexpr (n_test_components != 1) Base::eval_shape_div_on_cell(it, test_shape_grads_, test_divs);
                if constexpr (is_petrov_galerkin && n_trial_components != 1)
                    Base::eval_shape_div_on_cell(it, trial_shape_grads_, trial_divs);
            }
            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                if constexpr (!test_hess_is_zero) Base::eval_shape_hess_on_cell(it, test_shape_hess_, test_hess);
                if constexpr (is_petrov_galerkin && !trial_hess_is_zero)
                    Base::eval_shape_hess_on_cell(it, trial_shape_hess_, trial_hess);
            }

            // perform integration of weak form for (i, j)-th basis pair
            test_active_dofs = it->dofs();
            if constexpr (is_petrov_galerkin) { trial_active_dofs = trial_dof_handler()->active_dofs(it->id()); }
            for (int i = 0; i < n_trial_basis; ++i) {      // trial function loop
                for (int j = 0; j < n_test_basis; ++j) {   // test function loop
                    double value = 0;
                    for (int q_k = 0; q_k < n_quadrature_nodes; ++q_k) {
                        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_values)) {
                            fe_packet.trial_value.assign_inplace_from(trial_shape_values_.template slice<0, 1>(i, q_k));
                            fe_packet.test_value .assign_inplace_from(test_shape_values_ .template slice<0, 1>(j, q_k));
                        }
                        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                            fe_packet.trial_grad.assign_inplace_from(is_galerkin ?
                                test_grads.template slice<0, 1>(i, q_k) : trial_grads.template slice<0, 1>(i, q_k));
                            fe_packet.test_grad .assign_inplace_from(test_grads.template slice<0, 1>(j, q_k));
                        }
                        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                            if constexpr (n_trial_components != 1) {
                                fe_packet.trial_div =
                                  (is_galerkin && n_test_components != 1) ? test_divs(i, q_k) : trial_divs(i, q_k);
                            }
                            if constexpr (n_test_components != 1) fe_packet.test_div = test_divs(j, q_k);
                        }
                        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                            if constexpr (!trial_hess_is_zero)
                                fe_packet.trial_hess.assign_inplace_from(is_galerkin ?
				    test_hess.template slice<0, 1>(i, q_k) : trial_hess.template slice<0, 1>(i, q_k));
                            if constexpr (!test_hess_is_zero)
                                fe_packet.test_hess.assign_inplace_from(test_hess.template slice<0, 1>(j, q_k));
                        }
                        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
                            fe_packet.quad_node_id = local_cell_id * n_quadrature_nodes + q_k;
                        }
                        value += Quadrature::weights[q_k] * form_(fe_packet);
                    }
                    triplet_list.emplace_back(
                      test_active_dofs[j], is_galerkin ? test_active_dofs[i] : trial_active_dofs[i],
                      value * fe_packet.measure);
                }
            }
            local_cell_id++;
        }
        return;
    }

/*=====================================================================================================================================================================*/
/*==================== assemble sequenziale ma con lambda function per dimostrare che overhead è dato da calcolo nella lambda e non da threadpool====================*/
/*=====================================================================================================================================================================*/
/*=====================================================================================================================================================================*/
//assemble_lambda()
    Eigen::SparseMatrix<double> assemble_lambda() const {
        Eigen::SparseMatrix<double> assembled_mat(test_dof_handler()->n_dofs(), trial_dof_handler()->n_dofs());
        std::vector<Eigen::Triplet<double>> triplet_list;

        //riserva spazio per evitare riallocamento vettore
        int n_cell = this->Base::dof_handler_->triangulation()->n_cells();
        int triple_per_cella = n_trial_basis * n_test_basis; //9 qui;
        int tot_triple = n_cell * triple_per_cella;
        triplet_list.reserve(tot_triple);
auto start = std::chrono::high_resolution_clock::now();
    assemble_lambda(triplet_list);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
std::cout<<duration.count()<<",";
	// linearity of the integral is implicitly used here, as duplicated triplets are summed up (see Eigen docs)
        assembled_mat.setFromTriplets(triplet_list.begin(), triplet_list.end());
        assembled_mat.makeCompressed();

        return assembled_mat;
    }    
//assemble_lambda(...)
void assemble_lambda(std::vector<Eigen::Triplet<double>>& triplet_list) const {
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );
        // prepare assembly loop
	Eigen::Matrix<int, Dynamic, 1> test_active_dofs, trial_active_dofs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, embed_dim, n_test_components >> test_grads;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, embed_dim, n_trial_components>> trial_grads;
        Matrix<double, n_test_basis , n_quadrature_nodes> test_divs;
        Matrix<double, n_trial_basis, n_quadrature_nodes> trial_divs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, n_test_components,  embed_dim, embed_dim>>
	  test_hess;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, n_trial_components, embed_dim, embed_dim>>
          trial_hess;

        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
            Base::distribute_quadrature_nodes(begin, end);
        }
        // start assembly loop
        internals::fe_assembler_packet<embed_dim> fe_packet(n_trial_components, n_test_components);
	// if hessians are zero, assemble physical hessian once and never update
        constexpr bool test_hess_is_zero = std::all_of(
          test_shape_hess_.data(), test_shape_hess_.data() + test_shape_hess_.size(), [](double x) { return x == 0; });
        constexpr bool trial_hess_is_zero =
          std::all_of(trial_shape_hess_.data(), trial_shape_hess_.data() + trial_shape_hess_.size(), [](double x) {
              return x == 0;
          });
        if constexpr (test_hess_is_zero ) { std::fill_n(fe_packet.test_hess.data(), fe_packet.test_hess.size(), 0.0); }
        if constexpr (trial_hess_is_zero) {
            std::fill_n(fe_packet.trial_hess.data(), fe_packet.trial_hess.size(), 0.0);
        }
        //lambda function che calcola triple
        [&,this](){
            int local_cell_id = 0;
            for (iterator it = begin; it != end; ++it) {
                // update fe_packet content based on form requests
                fe_packet.measure = it->measure();
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_geo_id)) { fe_packet.geo_id = it->id(); }
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_face_normal)) {
                    fdapde_static_assert(Options_ == FaceMajor, BILINEAR_FORM_REQUIRES_A_FACE_MAJOR_ASSEMBLY_LOOP);
                    fe_packet.normal.assign_inplace_from(it->normal());
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                    Base::eval_shape_grads_on_cell(it, test_shape_grads_, test_grads);
                    if constexpr (is_petrov_galerkin) Base::eval_shape_grads_on_cell(it, trial_shape_grads_, trial_grads);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                    fdapde_static_assert(
                    n_test_components != 1 || n_trial_components != 1,
                    DIVERGENCE_OPERATOR_IS_DEFINED_ONLY_FOR_VECTOR_ELEMENTS);
                    if constexpr (n_test_components != 1) Base::eval_shape_div_on_cell(it, test_shape_grads_, test_divs);
                    if constexpr (is_petrov_galerkin && n_trial_components != 1)
                        Base::eval_shape_div_on_cell(it, trial_shape_grads_, trial_divs);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                    if constexpr (!test_hess_is_zero) Base::eval_shape_hess_on_cell(it, test_shape_hess_, test_hess);
                    if constexpr (is_petrov_galerkin && !trial_hess_is_zero)
                        Base::eval_shape_hess_on_cell(it, trial_shape_hess_, trial_hess);
                }

                // perform integration of weak form for (i, j)-th basis pair
                test_active_dofs = it->dofs();
                if constexpr (is_petrov_galerkin) { trial_active_dofs = trial_dof_handler()->active_dofs(it->id()); }
                for (int i = 0; i < n_trial_basis; ++i) {      // trial function loop
                    for (int j = 0; j < n_test_basis; ++j) {   // test function loop
                        double value = 0;
                        for (int q_k = 0; q_k < n_quadrature_nodes; ++q_k) {
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_values)) {
                                fe_packet.trial_value.assign_inplace_from(trial_shape_values_.template slice<0, 1>(i, q_k));
                                fe_packet.test_value .assign_inplace_from(test_shape_values_ .template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                                fe_packet.trial_grad.assign_inplace_from(is_galerkin ?
                                    test_grads.template slice<0, 1>(i, q_k) : trial_grads.template slice<0, 1>(i, q_k));
                                fe_packet.test_grad .assign_inplace_from(test_grads.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                                if constexpr (n_trial_components != 1) {
                                    fe_packet.trial_div =
                                    (is_galerkin && n_test_components != 1) ? test_divs(i, q_k) : trial_divs(i, q_k);
                                }
                                if constexpr (n_test_components != 1) fe_packet.test_div = test_divs(j, q_k);
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                                if constexpr (!trial_hess_is_zero)
                                    fe_packet.trial_hess.assign_inplace_from(is_galerkin ?
                        test_hess.template slice<0, 1>(i, q_k) : trial_hess.template slice<0, 1>(i, q_k));
                                if constexpr (!test_hess_is_zero)
                                    fe_packet.test_hess.assign_inplace_from(test_hess.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
                                fe_packet.quad_node_id = local_cell_id * n_quadrature_nodes + q_k;
                            }
                            value += Quadrature::weights[q_k] * form_(fe_packet);
                        }
                        triplet_list.emplace_back(
                        test_active_dofs[j], is_galerkin ? test_active_dofs[i] : trial_active_dofs[i],
                        value * fe_packet.measure);
                    }
                }
                local_cell_id++;
            }
        }();//eseguita subito
        return;
    }

/*================================================================================================================================================================================================================================*/
/*==============ASSEMBLE PARALLELO, UNICOVETTORE DI TRIPLE NON UNICHE=========================================================================================================================================================================================*/
/*================================================================================================================================================================================================================================*/
/*================================================================================================================================================================================================================================*/    
    //assemble parallelo con unicco vettore
//assemble()
    Eigen::SparseMatrix<double> assemble(execution::execution_parallel, fdapde::Threadpool<fdapde::steal::random>& Tp, int granularity = -1) const {
        Eigen::SparseMatrix<double> assembled_mat(test_dof_handler()->n_dofs(), trial_dof_handler()->n_dofs());
        
        int n_cell = this->Base::dof_handler_->triangulation()->n_cells();
        int triple_per_cella = n_trial_basis * n_test_basis; //9 qui;
        int tot_triple = n_cell * triple_per_cella;
        std::vector<Eigen::Triplet<double>> triplet_list(tot_triple);
auto start = std::chrono::high_resolution_clock::now();
	assemble(triplet_list,Tp,granularity);
    //assemble_nocopy(triplet_list,Tp,granularity);
    //assemble_unica_triple(triplet_list,Tp,granularity);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
std::cout<<duration.count()<<",";

	// linearity of the integral is implicitly used here, as duplicated triplets are summed up (see Eigen docs)
        assembled_mat.setFromTriplets(triplet_list.begin(), triplet_list.end());
        assembled_mat.makeCompressed();

        return assembled_mat;
    }

    //overload che crea threadpool al posto di averla in input
    Eigen::SparseMatrix<double> assemble(execution::execution_parallel,int n_thread = std::thread::hardware_concurrency(),int size_queue = 1024, int granularity = -1) const { //int kk per il momento in input per fare test piu comodamente. OSS: per ora visto che kk=1 fino a kk=10 non c'è differenza. se troppo alto invece peggioramento evidente (es kk=100)
        fdapde::Threadpool<fdapde::steal::random> Tp(size_queue,n_thread);
        return assemble(execution::par,Tp,granularity);
    }
    
 //assemble(...)   
    void assemble(std::vector<Eigen::Triplet<double>>& triplet_list,fdapde::Threadpool<fdapde::steal::random> &Tp, int granularity) const {
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );
        // prepare assembly loop
	Eigen::Matrix<int, Dynamic, 1> test_active_dofs, trial_active_dofs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, embed_dim, n_test_components >> test_grads;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, embed_dim, n_trial_components>> trial_grads;
        Matrix<double, n_test_basis , n_quadrature_nodes> test_divs;
        Matrix<double, n_trial_basis, n_quadrature_nodes> trial_divs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, n_test_components,  embed_dim, embed_dim>>
	  test_hess;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, n_trial_components, embed_dim, embed_dim>>
          trial_hess;

        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
            Base::distribute_quadrature_nodes(begin, end);
        }
        // start assembly loop
        internals::fe_assembler_packet<embed_dim> fe_packet(n_trial_components, n_test_components);
	// if hessians are zero, assemble physical hessian once and never update
        constexpr bool test_hess_is_zero = std::all_of(
          test_shape_hess_.data(), test_shape_hess_.data() + test_shape_hess_.size(), [](double x) { return x == 0; });
        constexpr bool trial_hess_is_zero =
          std::all_of(trial_shape_hess_.data(), trial_shape_hess_.data() + trial_shape_hess_.size(), [](double x) {
              return x == 0;
          });
        if constexpr (test_hess_is_zero ) { std::fill_n(fe_packet.test_hess.data(), fe_packet.test_hess.size(), 0.0); }
        if constexpr (trial_hess_is_zero) {
            std::fill_n(fe_packet.trial_hess.data(), fe_packet.trial_hess.size(), 0.0);
        }

        //paralleliziamo con parallel_for con defaul granularity = 1 e creiamo da qui i mini_for (cosi ogni iterazione è minifor e quindi anche se un job= 1 iterazione ogni ojob sara un minifor)
        int num_worker = Tp.get_n_worker();
        //numero celle
        int count = this->Base::dof_handler_->triangulation()->n_cells();
        //se defaul (input -1) allora messa granularity s.t. 1 job per worker (+1 job di resto ma con iterazioni < min(n_threads,granularity) quindi trascurabile)
        if(granularity == -1){
            granularity = (count/num_worker > 0) ? count/num_worker : 1;
        }
        int granularity_last_job = count % granularity;
        int n_job = (granularity_last_job == 0) ? count/granularity : count/granularity +1 ;
        
        Tp.parallel_for(0,n_job,[=,this,&Tp,&triplet_list](int ii)mutable{ //passare tutto come copia o reference ? ogni iterazione deve avere suo fe_packet ecc quindi copia. 
            int local_cell_id = ii*granularity; 
            iterator it = begin; //non serve copiare begin perché tanto è gia copia di begin esterno dentro lambda, poi sostituire a tutti it begin(solo per formalità non cambia niente ovviamente)
            it += (ii*granularity);
            //se ultimo job iterazioni sono resto 
            int granularity_job = (granularity_last_job != 0 && ii == n_job-1)? granularity_last_job : granularity; 
            int triple_per_cella = n_trial_basis * n_test_basis; 
            
            for (int l = 0; l<granularity_job; l++) {
                // update fe_packet content based on form requests
                fe_packet.measure = it->measure();
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_geo_id)) { fe_packet.geo_id = it->id(); }
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_face_normal)) {
                    fdapde_static_assert(Options_ == FaceMajor, BILINEAR_FORM_REQUIRES_A_FACE_MAJOR_ASSEMBLY_LOOP);
                    fe_packet.normal.assign_inplace_from(it->normal());
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                    Base::eval_shape_grads_on_cell(it, test_shape_grads_, test_grads);
                    if constexpr (is_petrov_galerkin) Base::eval_shape_grads_on_cell(it, trial_shape_grads_, trial_grads);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                    fdapde_static_assert(
                    n_test_components != 1 || n_trial_components != 1,
                    DIVERGENCE_OPERATOR_IS_DEFINED_ONLY_FOR_VECTOR_ELEMENTS);
                    if constexpr (n_test_components != 1) Base::eval_shape_div_on_cell(it, test_shape_grads_, test_divs);
                    if constexpr (is_petrov_galerkin && n_trial_components != 1)
                        Base::eval_shape_div_on_cell(it, trial_shape_grads_, trial_divs);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                    if constexpr (!test_hess_is_zero) Base::eval_shape_hess_on_cell(it, test_shape_hess_, test_hess);
                    if constexpr (is_petrov_galerkin && !trial_hess_is_zero)
                        Base::eval_shape_hess_on_cell(it, trial_shape_hess_, trial_hess);
                }

                // perform integration of weak form for (i, j)-th basis pair
                int index_global_triplet_list = local_cell_id*triple_per_cella;
                test_active_dofs = it->dofs();
                if constexpr (is_petrov_galerkin) { trial_active_dofs = trial_dof_handler()->active_dofs(it->id()); }
                for (int i = 0; i < n_trial_basis; ++i) {      // trial function loop
                    for (int j = 0; j < n_test_basis; ++j) {   // test function loop
                        double value = 0;
                        for (int q_k = 0; q_k < n_quadrature_nodes; ++q_k) {
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_values)) {
                                fe_packet.trial_value.assign_inplace_from(trial_shape_values_.template slice<0, 1>(i, q_k));
                                fe_packet.test_value .assign_inplace_from(test_shape_values_ .template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                                fe_packet.trial_grad.assign_inplace_from(is_galerkin ?
                                    test_grads.template slice<0, 1>(i, q_k) : trial_grads.template slice<0, 1>(i, q_k));
                                fe_packet.test_grad .assign_inplace_from(test_grads.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                                if constexpr (n_trial_components != 1) {
                                    fe_packet.trial_div =
                                    (is_galerkin && n_test_components != 1) ? test_divs(i, q_k) : trial_divs(i, q_k);
                                }
                                if constexpr (n_test_components != 1) fe_packet.test_div = test_divs(j, q_k);
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                                if constexpr (!trial_hess_is_zero)
                                    fe_packet.trial_hess.assign_inplace_from(is_galerkin ?
                        test_hess.template slice<0, 1>(i, q_k) : trial_hess.template slice<0, 1>(i, q_k));
                                if constexpr (!test_hess_is_zero)
                                    fe_packet.test_hess.assign_inplace_from(test_hess.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
                                fe_packet.quad_node_id = local_cell_id * n_quadrature_nodes + q_k; 
                            }
                            value += Quadrature::weights[q_k] * form_(fe_packet);
                        }
                        //threadsafe perché ogni worker scrive su suo [index_worker] e vettore esterno non si rialloca
                        Eigen::Triplet<double> tripla(test_active_dofs[j], is_galerkin ? test_active_dofs[i] : trial_active_dofs[i],value * fe_packet.measure);
                        triplet_list[index_global_triplet_list] = std::move(tripla);
                        index_global_triplet_list ++;
                    }
                }
                local_cell_id ++;
                ++it;
            }
            
        });
        
        return;
    }

//assmeble senza copie ma con inizializzazione di variabili temporanee e packet dentro ai job, altro modo sarebbe farne diretttamente uno per worker mettendoli dentro un vettore e ogni worker ha il suo 
    void assemble_nocopy(std::vector<Eigen::Triplet<double>>& triplet_list,fdapde::Threadpool<fdapde::steal::random> &Tp, int granularity) const {
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );

        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
            Base::distribute_quadrature_nodes(begin, end);
        }


        //paralleliziamo con parallel_for con defaul granularity = 1 e creiamo da qui i mini_for (cosi ogni iterazione è minifor e quindi anche se un job= 1 iterazione ogni ojob sara un minifor)
        int num_worker = Tp.get_n_worker();
        //numero celle
        int count = this->Base::dof_handler_->triangulation()->n_cells();
        //se defaul (input -1) allora messa granularity s.t. 1 job per worker (+1 job di resto ma con iterazioni < min(n_threads,granularity) quindi trascurabile)
        if(granularity == -1){
            granularity = (count/num_worker > 0) ? count/num_worker : 1;
        }
        int granularity_last_job = count % granularity;
        int n_job = (granularity_last_job == 0) ? count/granularity : count/granularity +1 ;
        
        Tp.parallel_for(0,n_job,[begin,end,num_worker,count,granularity,n_job,granularity_last_job,this,&Tp,&triplet_list](int ii)mutable{ //passare tutto come copia o reference ? ogni iterazione deve avere suo fe_packet ecc quindi copia. 
            //definizione ogetti temporanei per job
                    // prepare assembly loop
	Eigen::Matrix<int, Dynamic, 1> test_active_dofs, trial_active_dofs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, embed_dim, n_test_components >> test_grads;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, embed_dim, n_trial_components>> trial_grads;
        Matrix<double, n_test_basis , n_quadrature_nodes> test_divs;
        Matrix<double, n_trial_basis, n_quadrature_nodes> trial_divs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, n_test_components,  embed_dim, embed_dim>>
	  test_hess;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, n_trial_components, embed_dim, embed_dim>>
          trial_hess;

        // start assembly loop
        internals::fe_assembler_packet<embed_dim> fe_packet(n_trial_components, n_test_components);
	// if hessians are zero, assemble physical hessian once and never update
        constexpr bool test_hess_is_zero = std::all_of(
          test_shape_hess_.data(), test_shape_hess_.data() + test_shape_hess_.size(), [](double x) { return x == 0; });
        constexpr bool trial_hess_is_zero =
          std::all_of(trial_shape_hess_.data(), trial_shape_hess_.data() + trial_shape_hess_.size(), [](double x) {
              return x == 0;
          });
        if constexpr (test_hess_is_zero ) { std::fill_n(fe_packet.test_hess.data(), fe_packet.test_hess.size(), 0.0); }
        if constexpr (trial_hess_is_zero) {
            std::fill_n(fe_packet.trial_hess.data(), fe_packet.trial_hess.size(), 0.0);
        }
            int local_cell_id = ii*granularity; 
            iterator it = begin;
            it += (ii*granularity);
            //se ultimo job iterazioni sono resto 
            int granularity_job = (granularity_last_job != 0 && ii == n_job-1)? granularity_last_job : granularity; 
            int triple_per_cella = n_trial_basis * n_test_basis; 
            
            for (int l = 0; l<granularity_job; l++) {
                // update fe_packet content based on form requests
                fe_packet.measure = it->measure();
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_geo_id)) { fe_packet.geo_id = it->id(); }
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_face_normal)) {
                    fdapde_static_assert(Options_ == FaceMajor, BILINEAR_FORM_REQUIRES_A_FACE_MAJOR_ASSEMBLY_LOOP);
                    fe_packet.normal.assign_inplace_from(it->normal());
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                    Base::eval_shape_grads_on_cell(it, test_shape_grads_, test_grads);
                    if constexpr (is_petrov_galerkin) Base::eval_shape_grads_on_cell(it, trial_shape_grads_, trial_grads);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                    fdapde_static_assert(
                    n_test_components != 1 || n_trial_components != 1,
                    DIVERGENCE_OPERATOR_IS_DEFINED_ONLY_FOR_VECTOR_ELEMENTS);
                    if constexpr (n_test_components != 1) Base::eval_shape_div_on_cell(it, test_shape_grads_, test_divs);
                    if constexpr (is_petrov_galerkin && n_trial_components != 1)
                        Base::eval_shape_div_on_cell(it, trial_shape_grads_, trial_divs);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                    if constexpr (!test_hess_is_zero) Base::eval_shape_hess_on_cell(it, test_shape_hess_, test_hess);
                    if constexpr (is_petrov_galerkin && !trial_hess_is_zero)
                        Base::eval_shape_hess_on_cell(it, trial_shape_hess_, trial_hess);
                }

                // perform integration of weak form for (i, j)-th basis pair
                int index_global_triplet_list = local_cell_id*triple_per_cella;
                test_active_dofs = it->dofs();
                if constexpr (is_petrov_galerkin) { trial_active_dofs = trial_dof_handler()->active_dofs(it->id()); }
                for (int i = 0; i < n_trial_basis; ++i) {      // trial function loop
                    for (int j = 0; j < n_test_basis; ++j) {   // test function loop
                        double value = 0;
                        for (int q_k = 0; q_k < n_quadrature_nodes; ++q_k) {
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_values)) {
                                fe_packet.trial_value.assign_inplace_from(trial_shape_values_.template slice<0, 1>(i, q_k));
                                fe_packet.test_value .assign_inplace_from(test_shape_values_ .template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                                fe_packet.trial_grad.assign_inplace_from(is_galerkin ?
                                    test_grads.template slice<0, 1>(i, q_k) : trial_grads.template slice<0, 1>(i, q_k));
                                fe_packet.test_grad .assign_inplace_from(test_grads.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                                if constexpr (n_trial_components != 1) {
                                    fe_packet.trial_div =
                                    (is_galerkin && n_test_components != 1) ? test_divs(i, q_k) : trial_divs(i, q_k);
                                }
                                if constexpr (n_test_components != 1) fe_packet.test_div = test_divs(j, q_k);
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                                if constexpr (!trial_hess_is_zero)
                                    fe_packet.trial_hess.assign_inplace_from(is_galerkin ?
                        test_hess.template slice<0, 1>(i, q_k) : trial_hess.template slice<0, 1>(i, q_k));
                                if constexpr (!test_hess_is_zero)
                                    fe_packet.test_hess.assign_inplace_from(test_hess.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
                                fe_packet.quad_node_id = local_cell_id * n_quadrature_nodes + q_k; 
                            }
                            value += Quadrature::weights[q_k] * form_(fe_packet);
                        }
                        //threadsafe perché ogni worker scrive su suo [index_worker] e vettore esterno non si rialloca
                        Eigen::Triplet<double> tripla(test_active_dofs[j], is_galerkin ? test_active_dofs[i] : trial_active_dofs[i],value * fe_packet.measure);
                        triplet_list[index_global_triplet_list] = std::move(tripla);
                        index_global_triplet_list ++;
                    }
                }
                local_cell_id ++;
                ++it;
            }
            
        });
        
        return;
    }    


/*================================================================================================================================================================================================================================*/
/*==============ASSEMBLE PARALLELO, UNICA MATRICE, MA FORMATO NON EIGEN (FORMATO PESSIMO NON CACHE FRIENDLY)=========================================================================================================================================================================================*/
/*================================================================================================================================================================================================================================*/
/*================================================================================================================================================================================================================================*/    
    // in parallelo fa somma delle triple uguali e quindi calcolo matrice ma in formato diverso da eigen, quindi poi passaggio da matrice fatta da vettore<map<int,double>> a sparse matrix con prima copia di triple vector quindi risultato speedup totale metodo non buono 
//assemble_unicamatrix()    
    Eigen::SparseMatrix<double> assemble_unicamatrix(execution::execution_parallel, fdapde::Threadpool<fdapde::steal::random>& Tp, int kk = 1) const {
        Eigen::SparseMatrix<double> assembled_mat(test_dof_handler()->n_dofs(), trial_dof_handler()->n_dofs());
        
        int n_nodes = this->Base::dof_handler_->triangulation()->n_nodes();
        int n_worker = Tp.get_n_worker();
        //vettore dove ogni elemento i-esimo è riga i-esima di matrice, ogni vector[j] = mappa di <colonna,value> 
        //perché le righe sono tutte presenti (size vector a priori cosi threadsafe), le colonne sono sparse quindi no size a priori allora serve mutex per modifica mappe(aggiunta o somma a colonne) 
        std::vector<std::map<int,double>> matrix (n_nodes); //in caso generale non è nodes ma test_dof_handler()->n_dofs(), qui funziona perché ogni nodo corrisponde a test function. TODO: cambiare conn n_dofs  
        std::vector<std::mutex> mutexs (n_worker); //vettore di mutex per modifica parallela di matrxi comune, ogni mutex j associato a righe vettore di matrice da (j)*(tot_righe/n_worker) ad (j+1)*(tot_righe/n_worker) (ideale sarebbe ogni worker sblocca il suo mutex, ma dipende da divisione di celle)
        
	assemble_unicamatrix(matrix,Tp,mutexs);

        std::vector<Eigen::Triplet<double>> triplet_list;

        //reserve per evitare riallocamento, reserva spazio di tot triple anche se basta spazio di tot_triple uniche (todo: stima per eccesso ma minore di tot_triple sarebbe meglio)
        int n_cell = this->Base::dof_handler_->triangulation()->n_cells();
        int triple_per_cella = n_trial_basis * n_test_basis; 
        int tot_triple = n_cell * triple_per_cella;
        triplet_list.reserve(tot_triple);
        for(int r = 0; r<matrix.size(); r++){
            for(auto it = matrix[r].begin(); it != matrix[r].end(); it++){
                triplet_list.emplace_back(r,it->first,it->second);
            }
        }
        
        // linearity of the integral is implicitly used here, as duplicated triplets are summed up (see Eigen docs)
        assembled_mat.setFromTriplets(triplet_list.begin(), triplet_list.end());
        assembled_mat.makeCompressed();


        return assembled_mat;
    }
//assemble_unicamatrix(...)
    void assemble_unicamatrix(std::vector<std::map<int,double>>& matrix,fdapde::Threadpool<fdapde::steal::random> &Tp,std::vector<std::mutex>& mutexs) const {
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );
        // prepare assembly loop
	Eigen::Matrix<int, Dynamic, 1> test_active_dofs, trial_active_dofs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, embed_dim, n_test_components >> test_grads;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, embed_dim, n_trial_components>> trial_grads;
        Matrix<double, n_test_basis , n_quadrature_nodes> test_divs;
        Matrix<double, n_trial_basis, n_quadrature_nodes> trial_divs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, n_test_components,  embed_dim, embed_dim>>
	  test_hess;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, n_trial_components, embed_dim, embed_dim>>
          trial_hess;

        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
            Base::distribute_quadrature_nodes(begin, end);
        }
        // start assembly loop
        internals::fe_assembler_packet<embed_dim> fe_packet(n_trial_components, n_test_components);
	// if hessians are zero, assemble physical hessian once and never update
        constexpr bool test_hess_is_zero = std::all_of(
          test_shape_hess_.data(), test_shape_hess_.data() + test_shape_hess_.size(), [](double x) { return x == 0; });
        constexpr bool trial_hess_is_zero =
          std::all_of(trial_shape_hess_.data(), trial_shape_hess_.data() + trial_shape_hess_.size(), [](double x) {
              return x == 0;
          });
        if constexpr (test_hess_is_zero ) { std::fill_n(fe_packet.test_hess.data(), fe_packet.test_hess.size(), 0.0); }
        if constexpr (trial_hess_is_zero) {
            std::fill_n(fe_packet.trial_hess.data(), fe_packet.trial_hess.size(), 0.0);
        }

        //paralleliziamo con parallel_for con defaul granularity = 1 e creiamo da qui i mini_for (cosi ogni iterazione è minifor e quindi anche se un job= 1 iterazione ogni ojob sara un minifor)
        int num_worker = Tp.get_n_worker();
        
        //numero celle
        int count = this->Base::dof_handler_->triangulation()->n_cells();
        

        const int it_per_worker = ((count / num_worker) > 0)? (count / num_worker) : (0);
        const int it_per_worker_resto = count % num_worker;
        std::vector<int> it_per_workers(num_worker,it_per_worker);
        for(int i = 0; i<it_per_worker_resto; i++){
            it_per_workers[i]++; //add uno ai worker che fanno resto es:  3 worker A B C, 10 iterator--> resto = 1 cioe B C fanno 3 e A fa 3+1
        }

        Tp.parallel_for(0,num_worker,[=,this,&Tp,&matrix,&mutexs](int ii)mutable{ //passare tutto come copia o reference ? ogni iterazione deve avere suo fe_packet ecc quindi copia. TODO: passare copia di solo quello che serve es fe_packet ecc e non tutto =
            int n_nodes = this->Base::dof_handler_->triangulation()->n_nodes();
            int local_cell_id = 0;
            iterator it = begin;
            for (int l = 0; l<ii; l++){
                local_cell_id += it_per_workers[l];
                it+=it_per_workers[l];
            }
            int triple_per_cella = n_trial_basis * n_test_basis; //9; //hardcoded per il momento
            auto mutex_of_row = [num_worker, tot_row = n_nodes](int row) -> int {
                int row_per_worker = tot_row / num_worker;
                int indx = row / row_per_worker;
                return std::min(indx, num_worker - 1); // evita sforare per ultime righe maggiori di row_per_worker*(num_worker). es (tot_row = 15, num_worker=4, row_per_worker = 3, ma 13,14/3 fa 4 e max indice è 3 )
            };
            for (int l = 0; l<it_per_workers[ii]; l++) {
                // update fe_packet content based on form requests
                fe_packet.measure = it->measure();
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_geo_id)) { fe_packet.geo_id = it->id(); }
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_face_normal)) {
                    fdapde_static_assert(Options_ == FaceMajor, BILINEAR_FORM_REQUIRES_A_FACE_MAJOR_ASSEMBLY_LOOP);
                    fe_packet.normal.assign_inplace_from(it->normal());
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                    Base::eval_shape_grads_on_cell(it, test_shape_grads_, test_grads);
                    if constexpr (is_petrov_galerkin) Base::eval_shape_grads_on_cell(it, trial_shape_grads_, trial_grads);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                    fdapde_static_assert(
                    n_test_components != 1 || n_trial_components != 1,
                    DIVERGENCE_OPERATOR_IS_DEFINED_ONLY_FOR_VECTOR_ELEMENTS);
                    if constexpr (n_test_components != 1) Base::eval_shape_div_on_cell(it, test_shape_grads_, test_divs);
                    if constexpr (is_petrov_galerkin && n_trial_components != 1)
                        Base::eval_shape_div_on_cell(it, trial_shape_grads_, trial_divs);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                    if constexpr (!test_hess_is_zero) Base::eval_shape_hess_on_cell(it, test_shape_hess_, test_hess);
                    if constexpr (is_petrov_galerkin && !trial_hess_is_zero)
                        Base::eval_shape_hess_on_cell(it, trial_shape_hess_, trial_hess);
                }

                // perform integration of weak form for (i, j)-th basis pair
                test_active_dofs = it->dofs();
                if constexpr (is_petrov_galerkin) { trial_active_dofs = trial_dof_handler()->active_dofs(it->id()); }
                for (int i = 0; i < n_trial_basis; ++i) {      // trial function loop
                    for (int j = 0; j < n_test_basis; ++j) {   // test function loop
                        double value = 0;
                        for (int q_k = 0; q_k < n_quadrature_nodes; ++q_k) {
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_values)) {
                                fe_packet.trial_value.assign_inplace_from(trial_shape_values_.template slice<0, 1>(i, q_k));
                                fe_packet.test_value .assign_inplace_from(test_shape_values_ .template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                                fe_packet.trial_grad.assign_inplace_from(is_galerkin ?
                                    test_grads.template slice<0, 1>(i, q_k) : trial_grads.template slice<0, 1>(i, q_k));
                                fe_packet.test_grad .assign_inplace_from(test_grads.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                                if constexpr (n_trial_components != 1) {
                                    fe_packet.trial_div =
                                    (is_galerkin && n_test_components != 1) ? test_divs(i, q_k) : trial_divs(i, q_k);
                                }
                                if constexpr (n_test_components != 1) fe_packet.test_div = test_divs(j, q_k);
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                                if constexpr (!trial_hess_is_zero)
                                    fe_packet.trial_hess.assign_inplace_from(is_galerkin ?
                        test_hess.template slice<0, 1>(i, q_k) : trial_hess.template slice<0, 1>(i, q_k));
                                if constexpr (!test_hess_is_zero)
                                    fe_packet.test_hess.assign_inplace_from(test_hess.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
                                fe_packet.quad_node_id = local_cell_id * n_quadrature_nodes + q_k; 
                            }
                            value += Quadrature::weights[q_k] * form_(fe_packet);
                        }

                        int r = test_active_dofs[j];
                        int c = is_galerkin ? test_active_dofs[i] : trial_active_dofs[i];
                        std::unique_lock<std::mutex> lock(mutexs[mutex_of_row(r)]);
                        matrix[r][c]+=value * fe_packet.measure;
                    }
                }
                local_cell_id ++;
                ++it;
            }
            
        });
        
        return;
    }

/*================================================================================================================================================================================================================================*/
/*==============ASSEMBLE PARALLELO, UNICOVETTORE DI TRIPLE UNICHE, =========================================================================================================================================================================================*/
/*================================================================================================================================================================================================================================*/
/*================================================================================================================================================================================================================================*/    
// unico vettore triplet_list di triple uniche (worker scrivono direttamente sulla matrice finale quindi)
// triplet_list è fatto da numero_righe_matrice_finale * 7, numero_righe_matrice_finale= numero nodi (dof), 7 sono il massimo di colonne non vuote per riga in mesh triangoli strutturata, generalizzare questo 6 non so bene come 
// non si fa il reduce perché worker sommano direttamente contributo in tripple,
// per far scirvere a tutti i worker su vettore unico di triple uniche quindi serev blocco di mutex che protegge (piu mutex meno possibilità di contesa inutile (ideale un mutex per tripla ma infattibile)).
 
//assemble_unica_tripla()
    Eigen::SparseMatrix<double> assemble_unica_tripla(execution::execution_parallel, fdapde::Threadpool<fdapde::steal::random>& Tp, int granularity = -1) const {
        Eigen::SparseMatrix<double> assembled_mat(test_dof_handler()->n_dofs(), trial_dof_handler()->n_dofs());
        int n_rows = test_dof_handler()->n_dofs();
        //std::cout<<"numero righe: "<<n_rows<<", numero nodi"<<this->Base::dof_handler_->triangulation()->n_nodes()<<std::endl;
        int column_per_row = 7; //hard-coded 6 in triangoli in mesh strutturata
        int tot_triple =  n_rows * column_per_row;
        std::vector<Eigen::Triplet<double>> triplet_list(tot_triple);
        std::vector<std::mutex> mutexs (Tp.get_n_worker()); //vettore di mutex per modifica parallela di matrxi comune, ogni mutex j associato a righe vettore di matrice da (j)*(tot_righe/n_worker) ad (j+1)*(tot_righe/n_worker) (ideale sarebbe ogni worker sblocca il suo mutex, ma dipende da divisione di celle)
        std::vector<int> count_columns_in_row(n_rows,0);//contatore di colonne non nulle per righe. anchesso si aggiorna con mutex lock
        //con vettore di mutex meglio una granularity massima(-1) cosi piu distanti le celle dei worker (1 job per worker)
auto start = std::chrono::high_resolution_clock::now();
    assemble_unica_triple(triplet_list,mutexs,count_columns_in_row,column_per_row,n_rows,Tp,granularity);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
std::cout<<duration.count()<<",";

	// linearity of the integral is implicitly used here, as duplicated triplets are summed up (see Eigen docs)
        assembled_mat.setFromTriplets(triplet_list.begin(), triplet_list.end());
        assembled_mat.makeCompressed();

        return assembled_mat;
    }
//assemble_unica_tripla(...)
    void assemble_unica_triple(std::vector<Eigen::Triplet<double>>& triplet_list,std::vector<std::mutex>& mutexs,std::vector<int>& count_columns_in_row,int column_per_row,int n_rows,fdapde::Threadpool<fdapde::steal::random> &Tp, int granularity) const {
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );
        // prepare assembly loop
	Eigen::Matrix<int, Dynamic, 1> test_active_dofs, trial_active_dofs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, embed_dim, n_test_components >> test_grads;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, embed_dim, n_trial_components>> trial_grads;
        Matrix<double, n_test_basis , n_quadrature_nodes> test_divs;
        Matrix<double, n_trial_basis, n_quadrature_nodes> trial_divs;
        MdArray<double, MdExtents<n_test_basis,  n_quadrature_nodes, n_test_components,  embed_dim, embed_dim>>
	  test_hess;
        MdArray<double, MdExtents<n_trial_basis, n_quadrature_nodes, n_trial_components, embed_dim, embed_dim>>
          trial_hess;

        if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
            Base::distribute_quadrature_nodes(begin, end);
        }
        // start assembly loop
        internals::fe_assembler_packet<embed_dim> fe_packet(n_trial_components, n_test_components);
	// if hessians are zero, assemble physical hessian once and never update
        constexpr bool test_hess_is_zero = std::all_of(
          test_shape_hess_.data(), test_shape_hess_.data() + test_shape_hess_.size(), [](double x) { return x == 0; });
        constexpr bool trial_hess_is_zero =
          std::all_of(trial_shape_hess_.data(), trial_shape_hess_.data() + trial_shape_hess_.size(), [](double x) {
              return x == 0;
          });
        if constexpr (test_hess_is_zero ) { std::fill_n(fe_packet.test_hess.data(), fe_packet.test_hess.size(), 0.0); }
        if constexpr (trial_hess_is_zero) {
            std::fill_n(fe_packet.trial_hess.data(), fe_packet.trial_hess.size(), 0.0);
        }

        //paralleliziamo con parallel_for con defaul granularity = 1 e creiamo da qui i mini_for (cosi ogni iterazione è minifor e quindi anche se un job= 1 iterazione ogni ojob sara un minifor)
        int num_worker = Tp.get_n_worker();
        //numero celle
        int count = this->Base::dof_handler_->triangulation()->n_cells();
        //se defaul (input -1) allora messa granularity s.t. 1 job per worker (+1 job di resto ma con iterazioni < min(n_threads,granularity) quindi trascurabile)
        if(granularity == -1){
            granularity = (count/num_worker > 0) ? count/num_worker : 1;
        }
        int granularity_last_job = count % granularity;
        int n_job = (granularity_last_job == 0) ? count/granularity : count/granularity +1 ;

        Tp.parallel_for(0,n_job,[=,this,&Tp,&triplet_list,&mutexs,&count_columns_in_row](int ii)mutable{ 
            int local_cell_id = ii*granularity; 
            iterator it = begin; //non serve copiare begin perché tanto è gia copia di begin esterno dentro lambda, poi sostituire a tutti it begin(solo per formalità non cambia niente ovviamente)
            it += (ii*granularity);
            //se ultimo job iterazioni sono resto 
            int granularity_job = (granularity_last_job != 0 && ii == n_job-1)? granularity_last_job : granularity; 
            int triple_per_cella = n_trial_basis * n_test_basis; 
            //associa indice di mutex in mutexs da lock per ogni righa che si vuole modificare
            auto mutex_of_row = [num_worker, n_rows](int row) -> int {
                int row_per_worker = n_rows / num_worker;
                int indx = n_rows / row_per_worker;
                return std::min(indx, num_worker - 1); // evita sforare per ultime righe maggiori di row_per_worker*(num_worker). es (tot_row = 15, num_worker=4, row_per_worker = 3, ma 13,14/3 fa 4 e max indice è 3 )
            };
            for (int l = 0; l<granularity_job; l++) {
                // update fe_packet content based on form requests
                fe_packet.measure = it->measure();
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_geo_id)) { fe_packet.geo_id = it->id(); }
                if constexpr (Form::XprBits & int(geo_assembler_flags::compute_face_normal)) {
                    fdapde_static_assert(Options_ == FaceMajor, BILINEAR_FORM_REQUIRES_A_FACE_MAJOR_ASSEMBLY_LOOP);
                    fe_packet.normal.assign_inplace_from(it->normal());
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                    Base::eval_shape_grads_on_cell(it, test_shape_grads_, test_grads);
                    if constexpr (is_petrov_galerkin) Base::eval_shape_grads_on_cell(it, trial_shape_grads_, trial_grads);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                    fdapde_static_assert(
                    n_test_components != 1 || n_trial_components != 1,
                    DIVERGENCE_OPERATOR_IS_DEFINED_ONLY_FOR_VECTOR_ELEMENTS);
                    if constexpr (n_test_components != 1) Base::eval_shape_div_on_cell(it, test_shape_grads_, test_divs);
                    if constexpr (is_petrov_galerkin && n_trial_components != 1)
                        Base::eval_shape_div_on_cell(it, trial_shape_grads_, trial_divs);
                }
                if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                    if constexpr (!test_hess_is_zero) Base::eval_shape_hess_on_cell(it, test_shape_hess_, test_hess);
                    if constexpr (is_petrov_galerkin && !trial_hess_is_zero)
                        Base::eval_shape_hess_on_cell(it, trial_shape_hess_, trial_hess);
                }

                // perform integration of weak form for (i, j)-th basis pair
                int index_global_triplet_list = local_cell_id*triple_per_cella;
                test_active_dofs = it->dofs();
                if constexpr (is_petrov_galerkin) { trial_active_dofs = trial_dof_handler()->active_dofs(it->id()); }
                for (int i = 0; i < n_trial_basis; ++i) {      // trial function loop
                    for (int j = 0; j < n_test_basis; ++j) {   // test function loop
                        double value = 0;
                        for (int q_k = 0; q_k < n_quadrature_nodes; ++q_k) {
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_values)) {
                                fe_packet.trial_value.assign_inplace_from(trial_shape_values_.template slice<0, 1>(i, q_k));
                                fe_packet.test_value .assign_inplace_from(test_shape_values_ .template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_grad)) {
                                fe_packet.trial_grad.assign_inplace_from(is_galerkin ?
                                    test_grads.template slice<0, 1>(i, q_k) : trial_grads.template slice<0, 1>(i, q_k));
                                fe_packet.test_grad .assign_inplace_from(test_grads.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_div)) {
                                if constexpr (n_trial_components != 1) {
                                    fe_packet.trial_div =
                                    (is_galerkin && n_test_components != 1) ? test_divs(i, q_k) : trial_divs(i, q_k);
                                }
                                if constexpr (n_test_components != 1) fe_packet.test_div = test_divs(j, q_k);
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_shape_hess)) {
                                if constexpr (!trial_hess_is_zero)
                                    fe_packet.trial_hess.assign_inplace_from(is_galerkin ?
                        test_hess.template slice<0, 1>(i, q_k) : trial_hess.template slice<0, 1>(i, q_k));
                                if constexpr (!test_hess_is_zero)
                                    fe_packet.test_hess.assign_inplace_from(test_hess.template slice<0, 1>(j, q_k));
                            }
                            if constexpr (Form::XprBits & int(fe_assembler_flags::compute_physical_quad_nodes)) {
                                fe_packet.quad_node_id = local_cell_id * n_quadrature_nodes + q_k; 
                            }
                            value += Quadrature::weights[q_k] * form_(fe_packet);
                        }
                        
                        int r = test_active_dofs[j];
                        int c = is_galerkin ? test_active_dofs[i] : trial_active_dofs[i];
                        std::unique_lock<std::mutex> lock(mutexs[mutex_of_row(r)]);
                        bool new_c = true;
                        int c_in_r = count_columns_in_row[r]; //variabile salvata cosi da non dover andare a ripescarla ogni volta, credo piu efficiente in multithreading
                        for (int d=0; d<c_in_r; d++){
                            if (triplet_list[r*column_per_row + d].col() == c ){
                                //std::cout << "[UPDATE] r=" << r << " c=" << c << " valore vecchio=" << triplet_list[r*column_per_row + d].value() << " posizione=" << d << std::endl;
                                //per sommare valore devo creare tripla nuova (eigen triple ha solo metodi const)
                                double new_value= triplet_list[r*column_per_row + d].value();
                                //std::cout<<"new_value: "<< new_value<<std::endl;
                                new_value += (value * fe_packet.measure);
                                triplet_list[r*column_per_row + d] = Eigen::Triplet<double>(r,c,new_value);
                                new_c = false;
                                //std::cout << "[UPDATE] r=" << r << " c=" << c << " valore aggiornato=" << new_value << " posizione=" << d << std::endl;
                            }
                        }
                        if(new_c){
                            triplet_list[r*column_per_row + c_in_r] = Eigen::Triplet<double>(r,c,(value * fe_packet.measure));
                            count_columns_in_row[r]++;
                            //std::cout << "[NEW] r=" << r << " c=" << c << " valore =" << value * fe_packet.measure <<  std::endl;
                        }
                        //non fccio unlock mutex guard tanto va OOS riga dopo
                    }
                }
                local_cell_id ++;
                ++it;
            }
            
        });
        
        return;
    }


/*
    class sparse_matrix{// alloca per ogni riga colonne_non_nulle_per_riga, dato che dipende dalla mash: in triangoli strutturati es è 7. per questo non serve outerStarts di eigen sparse matrix perché sarebbe [0,7,14,21,28...]
                        // cosi che poi con colouring della mesh ogni worker scrive in riga senza sincornizzazione in parallelo durante il loop delle celle di ogni colore
        std::vector<double> value;   [r1 r1 r1 r1 r1 r1 r1 |r2 r2 r2 r2 r2 r2 |r3 r3 r3 r3 r3 r3 r3| ...] 7colonne per riga max in triangoli strutturati 
    std::vector<int> innerIndices;   [indici colonne riga1,| indx_col_riga2   | indx_col_riga2     | ...] indici di colonne
        std::vector<int> nnz; //per poi mettere in compress e togliere spazi

    };
*/
    //divide celle in gruppi t.c. ogni cella non condivide DOF con nessen altra cella di stesso gruppo,
    //e quindi in parallelo scorrendo un gruppo ogni worker puo scrivere senza sincronizzazione in matrice sparsa con elementi gia allocati (es: 7*numero_righe)
    auto colouring()const{ //ritorna: std::vector<std::vector<iterator>>
        using iterator = typename Base::fe_traits::dof_iterator;
        iterator begin(Base::begin_.index(), test_dof_handler(), Base::begin_.marker());
        iterator end  (Base::end_.index(),   test_dof_handler(), Base::end_.marker()  );
        Eigen::Matrix<int, Dynamic, 1> test_active_dofs;
        int nodes_per_cella = this->Base::dof_handler_->triangulation()->n_nodes_per_cell;
        int colori = 0; //numero di colori usati 
        std::vector<std::vector<iterator>> colore_di_cella;//indice di vettore esterno è colore, elementi di vettore interno sono iterator a celle di quel colore 
        std::map<int,std::set<int>> colori_vicino_nodo; //per ogni nodo int il set dei colori delle celle che lo condividono
        std::set<int> colori_vicini; //tmp per segnare colori vicini per ogni cella durante scorrimento dei nodi
        for(auto it = begin; it!= end; ++it){
            test_active_dofs = it->dofs(); // numero di nodi globale dei nodi di it
            std::set<int> colori_vicini;
            for(int i = 0; i<nodes_per_cella; i++){
                //inserisco nodo nella mappa nodi-colorivicini se non c'è
                auto it_nodo = colori_vicino_nodo.find(test_active_dofs[i]);
                if(it_nodo != colori_vicino_nodo.end()){
                    colori_vicini.insert(it_nodo->second.begin(),it_nodo->second.end()); //aggiungi colori vicini a nodo in colori vicino a cella 
                }else{//prima volta che incontro nodo
                    colori_vicino_nodo[test_active_dofs[i]]; //add nodo con set vuoto
                }
            }
            
            bool colorato = false;
            int col = 0;
            while(!colorato && col<colori){
                if(colori_vicini.find(col) == colori_vicini.end()){
                    //se colore non in colori_vicini diventa colore di cella
                    colore_di_cella[col].push_back(it);
                    //add colore di cella ai colori vicini dei suoi nodi 
                    for(int i = 0; i<nodes_per_cella; i++){
                        colori_vicino_nodo[test_active_dofs[i]].insert(col);
                    }
                    colorato = true;
                }
                col++;
            }
            if(!colorato){
                //aggiungere nuovo colore 
                colori ++;
                colore_di_cella.push_back({});
                //colorare cella
                colore_di_cella[colori-1].push_back(it);//colori è numero colori totali e valore di ultimo colore aggiunto
                //add colore di cella ai colori vicini dei suoi nodi 
                for(int i = 0; i<nodes_per_cella; i++){
                    colori_vicino_nodo[test_active_dofs[i]].insert(colori-1);
                }
            }
        }
/*
        //print per vedere 
        for(int cols= 0; cols<colore_di_cella.size(); cols++){
            std::cout<<"colore: "<<cols<<" comprende celle: ";
            for(auto& cella : colore_di_cella[cols]){
                std::cout<<" , "<<cella->id();
            }
            std::cout<<std::endl;
        }
*/
        return colore_di_cella;
    }

    constexpr int n_dofs() const { return trial_dof_handler()->n_dofs(); }
    constexpr int rows() const { return test_dof_handler()->n_dofs(); }
    constexpr int cols() const { return trial_dof_handler()->n_dofs(); }
    constexpr const TrialSpace& trial_space() const { return *trial_space_; }
};


// optimized computation of discretized laplace operator (\int_D (\grad{\psi_i} * \grad{\psi_j})) for scalar elements
template <typename DofHandler, typename FeType> class scalar_fe_grad_grad_assembly_loop {
    static constexpr int local_dim = DofHandler::local_dim;
    static constexpr int embed_dim = DofHandler::embed_dim;
    using Quadrature = typename FeType::template cell_quadrature_t<local_dim>;
    using cell_dof_descriptor = FeType::template cell_dof_descriptor<local_dim>;
    using BasisType = typename cell_dof_descriptor::BasisType;
    static constexpr int n_quadrature_nodes = Quadrature::order;
    static constexpr int n_basis = BasisType::n_basis;
    static constexpr int n_components = FeType::n_components;
    fdapde_static_assert(n_components == 1, THIS_CLASS_IS_FOR_SCALAR_FINITE_ELEMENTS_ONLY);
    // compile-time evaluation of \nabla{\psi_i}(q_j), i = 1, ..., n_basis, j = 1, ..., n_quadrature_nodes
    static constexpr std::array<Matrix<double, local_dim, n_quadrature_nodes>, n_basis> shape_grad_ {[]() {
        std::array<Matrix<double, local_dim, n_quadrature_nodes>, n_basis> shape_grad_ {};
        BasisType basis {cell_dof_descriptor().dofs_phys_coords()};
        for (int i = 0; i < n_basis; ++i) {
            // evaluation of \nabla{\psi_i} at q_j, j = 1, ..., n_quadrature_nodes
            std::array<double, n_quadrature_nodes * local_dim> grad_eval_ {};
            for (int k = 0; k < n_quadrature_nodes; ++k) {
                auto grad = basis[i].gradient()(Quadrature::nodes.row(k).transpose());
                for (int j = 0; j < local_dim; ++j) { grad_eval_[j * n_quadrature_nodes + k] = grad[j]; }
            }
            shape_grad_[i] = Matrix<double, local_dim, n_quadrature_nodes>(grad_eval_);
        }
        return shape_grad_;
    }()};
    DofHandler* dof_handler_;
   public:
    scalar_fe_grad_grad_assembly_loop() = default;
    scalar_fe_grad_grad_assembly_loop(DofHandler& dof_handler) : dof_handler_(&dof_handler) { }
    Eigen::SparseMatrix<double> assemble() {
        if (!dof_handler_) dof_handler_->enumerate(FeType {});
        int n_dofs = dof_handler_->n_dofs();
        Eigen::SparseMatrix<double> assembled_mat(n_dofs, n_dofs);
        // prepare assembly loop
        std::vector<Eigen::Triplet<double>> triplet_list;
        Eigen::Matrix<int, Dynamic, 1> active_dofs;
        std::array<Matrix<double, embed_dim, n_quadrature_nodes>, n_basis> shape_grad;
        for (typename DofHandler::cell_iterator it = dof_handler_->cells_begin(); it != dof_handler_->cells_end();
             ++it) {
            active_dofs = it->dofs();
            // map reference cell shape functions' gradient on physical cell
            for (int i = 0; i < n_basis; ++i) {
                for (int j = 0; j < n_quadrature_nodes; ++j) {
                    shape_grad[i].col(j) =
                      Map<const double, local_dim, embed_dim>(it->invJ().data()).transpose() * shape_grad_[i].col(j);
                }
            }
            for (int i = 0; i < BasisType::n_basis; ++i) {
                for (int j = 0; j < i + 1; ++j) {
                    std::pair<const int&, const int&> minmax(std::minmax(active_dofs[i], active_dofs[j]));
                    double value = 0;
                    for (int k = 0; k < n_quadrature_nodes; ++k) {
                        value += Quadrature::weights[k] * (shape_grad[i].col(k)).dot(shape_grad[j].col(k));
                    }
                    triplet_list.emplace_back(minmax.first, minmax.second, value * it->measure());
                }
            }
        }
        // linearity of the integral is implicitly used here, as duplicated triplets are summed up (see Eigen docs)
        assembled_mat.setFromTriplets(triplet_list.begin(), triplet_list.end());
        assembled_mat.makeCompressed();
	return assembled_mat.selfadjointView<Eigen::Upper>();
    }
    constexpr int n_dofs() const { return dof_handler_->n_dofs(); }
    constexpr int rows() const { return n_dofs(); }
    constexpr int cols() const { return n_dofs(); }  
};
  
}   // namespace internals
}   // namespace fdapde

#endif   // __FDAPDE_FE_BILINEAR_FORM_ASSEMBLER_H__
