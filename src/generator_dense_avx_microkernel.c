/******************************************************************************
** Copyright (c) 2014-2015, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "generator_dense_avx_microkernel.h"

void libxsmm_generator_dense_avx_microkernel( libxsmm_generated_code*             io_generated_code,
                                              const libxsmm_gp_reg_mapping*       i_gp_reg_mapping,
                                              const libxsmm_micro_kernel_config*  i_micro_kernel_config,
                                              const libxsmm_xgemm_descriptor*     i_xgemm_desc,
                                              const unsigned int                  i_m_blocking,
                                              const unsigned int                  i_n_blocking,
                                              const int                           i_offset ) {
  /* @TODO fix this test */
#ifndef NDEBUG
  if ( (i_n_blocking > 3) || (i_n_blocking < 1) ) {
    fprintf(stderr, "LIBXSMM ERROR, libxsmm_generator_dense_avx2_microkernel, i_n_blocking smaller 1 or larger 3!!!\n");
    exit(-1);
  }
  /* test that l_m_blocking % i_micro_kernel_config->vector_length is 0 */
#endif
  /* deriving register blocking from kernel config */ 
  unsigned int l_m_blocking = i_m_blocking/i_micro_kernel_config->vector_length;
  /* register blocking counter in n */
  unsigned int l_n = 0;
  /* register blocking counter in m */
  unsigned int l_m = 0;
  /* start register of accumulator */
  unsigned int l_vec_reg_acc_start = 16 - (i_n_blocking * l_m_blocking);

  if (l_m_blocking == 1) {
    /* load column vectors of A */
    libxsmm_instruction_vec_move( io_generated_code, 
                                  i_micro_kernel_config->a_vmove_instruction, 
                                  i_gp_reg_mapping->gp_reg_a, 
                                  0, 
                                  i_micro_kernel_config->vector_name, 
                                  i_n_blocking, 0 );
    /* loop over columns of B */
    for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
      /* post increment of a pointer early */
      if ( (l_n == 0) ) {
        libxsmm_instruction_alu_imm( io_generated_code,
                                     i_micro_kernel_config->alu_add_instruction,
                                     i_gp_reg_mapping->gp_reg_a, 
                                     (i_xgemm_desc->lda)*(i_micro_kernel_config->datatype_size) );
      }
      /* different ways of using B */
      if ( i_offset != (-1) ) {
        libxsmm_instruction_vec_move( io_generated_code, 
                                      i_micro_kernel_config->b_vmove_instruction, 
                                      i_gp_reg_mapping->gp_reg_b, 
                                      ((i_micro_kernel_config->datatype_size) * i_offset) + (i_xgemm_desc->ldb * l_n * (i_micro_kernel_config->datatype_size)), 
                                      i_micro_kernel_config->vector_name, 
                                      l_n, 0 );
      } else {
        libxsmm_instruction_vec_move( io_generated_code, 
                                      i_micro_kernel_config->b_vmove_instruction, 
                                      i_gp_reg_mapping->gp_reg_b, 
                                      i_xgemm_desc->ldb * l_n *  i_micro_kernel_config->datatype_size, 
                                      i_micro_kernel_config->vector_name, 
                                      l_n, 0 );
        if ( l_n == (i_n_blocking -1) ) {
          libxsmm_instruction_alu_imm( io_generated_code,
                                       i_micro_kernel_config->alu_add_instruction, 
                                       i_gp_reg_mapping->gp_reg_b,
                                       i_micro_kernel_config->datatype_size );
        }
      }
      /* issue mul-add */
      libxsmm_instruction_vec_compute_reg( io_generated_code, 
                                           i_micro_kernel_config->vmul_instruction, 
                                           i_micro_kernel_config->vector_name, 
                                           i_n_blocking, 
                                           l_n, 
                                           i_n_blocking + l_n + 1 );
      libxsmm_instruction_vec_compute_reg( io_generated_code, 
                                           i_micro_kernel_config->vadd_instruction, 
                                           i_micro_kernel_config->vector_name, 
                                           i_n_blocking + l_n + 1, 
                                           l_vec_reg_acc_start + l_n, 
                                           l_vec_reg_acc_start + l_n );
    }
  } else {
    /* broadcast from B -> into vec registers 0 to i_n_blocking */
    if ( i_offset != (-1) ) { 
      for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
        libxsmm_instruction_vec_move( io_generated_code, 
                                      i_micro_kernel_config->b_vmove_instruction, 
                                      i_gp_reg_mapping->gp_reg_b, 
                                      ((i_micro_kernel_config->datatype_size) * i_offset) + (i_xgemm_desc->ldb * l_n * (i_micro_kernel_config->datatype_size)), 
                                      i_micro_kernel_config->vector_name, 
                                      l_n, 0 );
      }
    } else {
      for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
        libxsmm_instruction_vec_move( io_generated_code, 
                                      i_micro_kernel_config->b_vmove_instruction, 
                                      i_gp_reg_mapping->gp_reg_b, 
                                      i_xgemm_desc->ldb * l_n *  i_micro_kernel_config->datatype_size, 
                                      i_micro_kernel_config->vector_name, 
                                      l_n, 0 );
     }
     libxsmm_instruction_alu_imm( io_generated_code,
                                  i_micro_kernel_config->alu_add_instruction, 
                                  i_gp_reg_mapping->gp_reg_b,
                                  i_micro_kernel_config->datatype_size );
    }

    if (l_m_blocking == 3) {
      /* load column vectors of A and multiply with all broadcasted row entries of B */
      for ( l_m = 0; l_m < l_m_blocking ; l_m++ ) {
        libxsmm_instruction_vec_move( io_generated_code, 
                                      i_micro_kernel_config->a_vmove_instruction, 
                                      i_gp_reg_mapping->gp_reg_a, 
                                      (i_micro_kernel_config->datatype_size) * (i_micro_kernel_config->vector_length) * l_m, 
                                      i_micro_kernel_config->vector_name, 
                                      i_n_blocking , 0 );

        for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
          /* post increment early */
          if ( (l_m == (l_m_blocking-1)) && (l_n == 0) ) {
            libxsmm_instruction_alu_imm( io_generated_code,
                                         i_micro_kernel_config->alu_add_instruction,
                                         i_gp_reg_mapping->gp_reg_a, 
                                         (i_xgemm_desc->lda)*(i_micro_kernel_config->datatype_size) );
          }
          /* issue mul+add */
          libxsmm_instruction_vec_compute_reg( io_generated_code, 
                                               i_micro_kernel_config->vmul_instruction, 
                                               i_micro_kernel_config->vector_name, 
                                               i_n_blocking, 
                                               l_n, 
                                               i_n_blocking + l_m + 1 );
          libxsmm_instruction_vec_compute_reg( io_generated_code, 
                                               i_micro_kernel_config->vadd_instruction, 
                                               i_micro_kernel_config->vector_name, 
                                               i_n_blocking + l_m + 1, 
                                               l_vec_reg_acc_start + l_m + (l_m_blocking * l_n), 
                                               l_vec_reg_acc_start + l_m + (l_m_blocking * l_n) );
        }
      }
    } else {
      /* load column vectors of A and multiply with all broadcasted row entries of B */
      for ( l_m = 0; l_m < l_m_blocking ; l_m++ ) {
        libxsmm_instruction_vec_move( io_generated_code, 
                                      i_micro_kernel_config->a_vmove_instruction, 
                                      i_gp_reg_mapping->gp_reg_a, 
                                      (i_micro_kernel_config->datatype_size) * (i_micro_kernel_config->vector_length) * l_m, 
                                      i_micro_kernel_config->vector_name, 
                                      i_n_blocking+l_m, 0 );
      }
      for ( l_m = 0; l_m < l_m_blocking ; l_m++ ) {
        for ( l_n = 0; l_n < i_n_blocking; l_n++ ) {
          /* post increment early */
          if ( (l_m == (l_m_blocking-1)) && (l_n == 0) ) {
            libxsmm_instruction_alu_imm( io_generated_code,
                                         i_micro_kernel_config->alu_add_instruction,
                                         i_gp_reg_mapping->gp_reg_a, 
                                         (i_xgemm_desc->lda)*(i_micro_kernel_config->datatype_size) );
          }
          /* issue mul/add */
          libxsmm_instruction_vec_compute_reg( io_generated_code, 
                                               i_micro_kernel_config->vmul_instruction, 
                                               i_micro_kernel_config->vector_name, 
                                               i_n_blocking + l_m, 
                                               l_n, 
                                               i_n_blocking + l_m_blocking + l_m );
          libxsmm_instruction_vec_compute_reg( io_generated_code, 
                                               i_micro_kernel_config->vadd_instruction, 
                                               i_micro_kernel_config->vector_name, 
                                               i_n_blocking + l_m_blocking + l_m, 
                                               l_vec_reg_acc_start + l_m + (l_m_blocking * l_n) , 
                                               l_vec_reg_acc_start + l_m + (l_m_blocking * l_n) );
        }
      }
    }
  }
}

