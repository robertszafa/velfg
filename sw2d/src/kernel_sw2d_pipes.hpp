#include "CL/sycl/access/access.hpp"
#include "CL/sycl/properties/accessor_properties.hpp"
#include <CL/sycl.hpp>
#include <iostream>
#include <vector>

#if FPGA || FPGA_EMULATOR
#include <CL/sycl/INTEL/fpga_extensions.hpp>
#endif

#include "sw2d_sizes.hpp"

using namespace sycl;

void sw2d_pipes(queue &q, const std::vector<int> &wet, const std::vector<float> &eta,
                const std::vector<float> &u, const std::vector<float> &v,
                const std::vector<float> &h, std::vector<float> &etann, std::vector<float> &un,
                std::vector<float> &vn) {
  range<1> array_range{ARRAY_SIZE};

  buffer wet_buf(wet);
  buffer eta_buf(eta);
  buffer u_buf(u);
  buffer v_buf(v);
  buffer h_buf(h);
  // Output
  buffer etann_buf(etann);
  buffer un_buf(un);
  buffer vn_buf(vn);

  // Pipe declarations.
  using u_pipe = pipe<class u_pipe_class, float>;
  using v_pipe = pipe<class v_pipe_class, float>;
  using h_pipe = pipe<class h_pipe_class, float>;
  using eta_pipe = pipe<class eta_pipe_class, float>;
  using wet_pipe = pipe<class wet_pipe_class, int>;

  using eta_pipe_j_k = pipe<class eta_pipe_j_k_class, float>;
  using eta_pipe_jp1_k = pipe<class eta_pipe_jp1_k_class, float>;
  using eta_pipe_j_kp1 = pipe<class eta_pipe_j_kp1_class, float>;

  using du___dyn_pipe_j_k = pipe<class du___dyn_pipe_j_k_class, float>;
  using dv___dyn_pipe_j_k = pipe<class dv___dyn_pipe_j_k_class, float>;

  using wet_pipe_j_k = pipe<class wet_pipe_j_k_class, int>;
  using wet_pipe_j_kp1 = pipe<class wet_pipe_j_kp1_class, int>;
  using wet_pipe_jp1_k = pipe<class wet_pipe_jp1_k_class, int>;

  using vn_pipe = pipe<class vn_pipe_class, float>;
  using un_pipe = pipe<class un_pipe_class, float>;

  using h_pipe_j_k = pipe<class h_pipe_j_k_class, float>;
  using h_pipe_j_kp1 = pipe<class h_pipe_j_kp1_class, float>;
  using h_pipe_jp1_k = pipe<class h_pipe_jp1_k_class, float>;
  using h_pipe_j_km1 = pipe<class h_pipe_j_km1_class, float>;
  using h_pipe_jm1_k = pipe<class h_pipe_jm1_k_class, float>;
  using vn_pipe_j_k = pipe<class vn_pipe_j_k_class, float>;
  using vn_pipe_j_km1 = pipe<class vn_pipe_j_k_class, float>;
  using vn_pipe_jm1_k = pipe<class vn_pipe_j_k_class, float>;
  using un_pipe_j_k = pipe<class vn_pipe_j_k_class, float>;
  using un_pipe_j_km1 = pipe<class vn_pipe_j_k_class, float>;
  using un_pipe_jm1_k = pipe<class vn_pipe_j_k_class, float>;
  using eta_pipe_j_k_2 = pipe<class eta_pipe_j_k_2_class, float>;

  using etan_pipe = pipe<class etan_pipe_class, float>;
  using wet_pipe_2 = pipe<class wet_pipe_2_class, int>;

  using wet_pipe_2_j_k = pipe<class wet_pipe_2_j_k_class, float>;
  using wet_pipe_2_j_kp1 = pipe<class wet_pipe_2_j_kp1_class, float>;
  using wet_pipe_2_jp1_k = pipe<class wet_pipe_2_jp1_k_class, float>;
  using wet_pipe_2_j_km1 = pipe<class wet_pipe_2_j_km1_class, float>;
  using wet_pipe_2_jm1_k = pipe<class wet_pipe_2_jm1_k_class, float>;
  using etan_pipe_j_k = pipe<class etan_pipe_j_k_class, float>;
  using etan_pipe_j_kp1 = pipe<class etan_pipe_j_kp1_class, float>;
  using etan_pipe_jp1_k = pipe<class etan_pipe_jp1_k_class, float>;
  using etan_pipe_j_km1 = pipe<class etan_pipe_j_km1_class, float>;
  using etan_pipe_jm1_k = pipe<class etan_pipe_jm1_k_class, float>;

  q.submit([&](handler &hnd) {
    accessor wet(wet_buf, hnd, read_only);
    accessor eta(eta_buf, hnd, read_only);
    accessor u(u_buf, hnd, read_only);
    accessor v(v_buf, hnd, read_only);
    accessor h(h_buf, hnd, read_only);

    hnd.single_task<class memrd>([=]() {
      for (int i = 0; i < ARRAY_SIZE; ++i) {
        u_pipe::write(u[i]);
        v_pipe::write(v[i]);
        h_pipe::write(h[i]);
        eta_pipe::write(eta[i]);
        wet_pipe::write(wet[i]);
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class eta_smache>([=]() {
      constexpr uint REACH_POS = std::max(OFFSET_J_KP1, OFFSET_JP1_K);
      constexpr uint REACH_NEG = 0;
      constexpr uint STENCIL_REACH = REACH_NEG + REACH_POS;

      // Store the stencil reach + the current i_j_k element
      float eta_buffer[STENCIL_REACH + 1];

      for (int count = 0; count < (ARRAY_SIZE + STENCIL_REACH); count++) {
// SHIFT-RIGHT register for the buffers TODO: Probably no need for this pragma.
#pragma unroll
        for (int i = 0; i < STENCIL_REACH; ++i) {
          eta_buffer[i] = eta_buffer[i + 1];
        }

        if (count < DOMAIN_SIZE) {
          eta_buffer[STENCIL_REACH] = eta_pipe::read();
        }

        // start emitting once all data covered by the stencil reach is read into the buffer
        constexpr uint IDX_J_K = REACH_NEG;
        if (count >= STENCIL_REACH) {
          eta_pipe_j_k::write(eta_buffer[IDX_J_K]);
          eta_pipe_j_kp1::write(eta_buffer[IDX_J_K + OFFSET_J_KP1]);
          eta_pipe_jp1_k::write(eta_buffer[IDX_J_K + OFFSET_JP1_K]);
        }
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class map49>([=]() {
      for (int global_id = 0; global_id < ARRAY_SIZE; ++global_id) {
        auto eta_j_k = eta_pipe_j_k::read();
        auto eta_j_kp1 = eta_pipe_j_kp1::read();
        auto eta_jp1_k = eta_pipe_jp1_k::read();

        du___dyn_pipe_j_k::write(-dt * g * (eta_j_kp1 - eta_j_k) / dx);
        dv___dyn_pipe_j_k::write(-dt * g * (eta_jp1_k - eta_j_k) / dy);

        eta_pipe_j_k_2::write(eta_j_k);
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class wet_smache>([=]() {
      constexpr uint REACH_POS = std::max(OFFSET_J_KP1, OFFSET_JP1_K);
      constexpr uint REACH_NEG = 0;
      constexpr uint STENCIL_REACH = REACH_NEG + REACH_POS;

      // Store the stencil reach + the current i_j_k element
      float wet_buffer[STENCIL_REACH + 1];

      for (int count = 0; count < (ARRAY_SIZE + STENCIL_REACH); count++) {
// SHIFT-RIGHT register for the buffers
// TODO: Probably no need for this pragma.
#pragma unroll
        for (int i = 0; i < STENCIL_REACH; ++i) {
          wet_buffer[i] = wet_buffer[i + 1];
        }

        if (count < DOMAIN_SIZE) {
          wet_buffer[STENCIL_REACH] = wet_pipe::read();
        }

        // start emitting once all data covered by the stencil reach is read into the buffer
        constexpr uint IDX_J_K = REACH_NEG;
        if (count >= STENCIL_REACH) {
          wet_pipe_j_k::write(wet_buffer[IDX_J_K]);
          wet_pipe_j_kp1::write(wet_buffer[IDX_J_K + OFFSET_J_KP1]);
          wet_pipe_jp1_k::write(wet_buffer[IDX_J_K + OFFSET_JP1_K]);
        }
      }
    });
  });

  q.submit([&](handler &hnd) {
    accessor un(un_buf, hnd, write_only, no_init);
    accessor vn(vn_buf, hnd, write_only, no_init);

    hnd.single_task<class map55>([=]() {
      for (int global_id = 0; global_id < ARRAY_SIZE; ++global_id) {
        auto wet_j_k = wet_pipe_j_k::read();
        auto wet_jp1_k = wet_pipe_jp1_k::read();
        auto wet_j_kp1 = wet_pipe_j_kp1::read();
        auto du___dyn_j_k = du___dyn_pipe_j_k::read();
        auto dv___dyn_j_k = dv___dyn_pipe_j_k::read();
        auto u_j_k = u_pipe::read();
        auto v_j_k = v_pipe::read();

        float un_j_k = 0.0;
        if (wet_j_k == 1) {
          if ((wet_j_kp1 == 1) || (du___dyn_j_k > 0.0))
            un_j_k = u_j_k + du___dyn_j_k;
        } else {
          if ((wet_j_kp1 == 1) && (du___dyn_j_k < 0.0))
            un_j_k = u_j_k + du___dyn_j_k;
        }
        float vn_j_k = 0.0;
        if (wet_j_k == 1) {
          if ((wet_jp1_k == 1) || (dv___dyn_j_k > 0.0))
            vn_j_k = v_j_k + dv___dyn_j_k;
        } else {
          if ((wet_jp1_k == 1) && (dv___dyn_j_k < 0.0))
            vn_j_k = v_j_k + dv___dyn_j_k;
        }

        vn_pipe::write(vn_j_k);
        un_pipe::write(un_j_k);
        wet_pipe_2::write(wet_j_k);

        un[global_id] = un_j_k;
        vn[global_id] = vn_j_k;
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class h_smache>([=]() {
      constexpr uint REACH_POS = std::max(OFFSET_J_KP1, OFFSET_JP1_K);
      constexpr uint REACH_NEG = std::max(OFFSET_J_KM1, OFFSET_JM1_K);
      constexpr uint STENCIL_REACH = REACH_NEG + REACH_POS;

      // Store the stencil reach + the current i_j_k element
      float h_buffer[STENCIL_REACH + 1];

      for (int count = 0; count < (ARRAY_SIZE + STENCIL_REACH); count++) {
// SHIFT-RIGHT register for the buffers
// TODO: Probably no need for this pragma.
#pragma unroll
        for (int i = 0; i < STENCIL_REACH; ++i) {
          h_buffer[i] = h_buffer[i + 1];
        }

        if (count < DOMAIN_SIZE) {
          h_buffer[STENCIL_REACH] = h_pipe::read();
        }

        // start emitting once all data covered by the stencil reach is read into the buffer
        constexpr uint IDX_J_K = REACH_NEG;
        if (count >= STENCIL_REACH) {
          h_pipe_j_k::write(h_buffer[IDX_J_K]);
          h_pipe_j_kp1::write(h_buffer[IDX_J_K + OFFSET_J_KP1]);
          h_pipe_jp1_k::write(h_buffer[IDX_J_K + OFFSET_JP1_K]);
          h_pipe_j_km1::write(h_buffer[IDX_J_K - OFFSET_J_KM1]);
          h_pipe_jm1_k::write(h_buffer[IDX_J_K - OFFSET_JM1_K]);
        }
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class vn_un_smache>([=]() {
      constexpr uint REACH_POS = 0;
      constexpr uint REACH_NEG = std::max(OFFSET_J_KM1, OFFSET_JM1_K);
      constexpr uint STENCIL_REACH = REACH_NEG + REACH_POS;

      // Store the stencil reach + the current i_j_k element
      float vn_buffer[STENCIL_REACH + 1];
      float un_buffer[STENCIL_REACH + 1];

      for (int count = 0; count < (ARRAY_SIZE + STENCIL_REACH); count++) {
// SHIFT-RIGHT register for the buffers
// TODO: Probably no need for this pragma.
#pragma unroll
        for (int i = 0; i < STENCIL_REACH; ++i) {
          vn_buffer[i] = vn_buffer[i + 1];
          un_buffer[i] = un_buffer[i + 1];
        }

        if (count < DOMAIN_SIZE) {
          vn_buffer[STENCIL_REACH] = vn_pipe::read();
          un_buffer[STENCIL_REACH] = un_pipe::read();
        }

        // start emitting once all data covered by the stencil reach is read into the buffer
        constexpr uint IDX_J_K = REACH_NEG;
        if (count >= STENCIL_REACH) {
          vn_pipe_j_k::write(vn_buffer[IDX_J_K]);
          vn_pipe_j_km1::write(vn_buffer[IDX_J_K - OFFSET_J_KM1]);
          vn_pipe_jm1_k::write(vn_buffer[IDX_J_K - OFFSET_JM1_K]);

          un_pipe_j_k::write(un_buffer[IDX_J_K]);
          un_pipe_j_km1::write(un_buffer[IDX_J_K - OFFSET_J_KM1]);
          un_pipe_jm1_k::write(un_buffer[IDX_J_K - OFFSET_JM1_K]);
        }
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class map75>([=]() {
      for (int global_id = 0; global_id < ARRAY_SIZE; ++global_id) {
        auto h_j_k = h_pipe_j_k::read();
        auto h_j_kp1 = h_pipe_j_kp1::read();
        auto h_jp1_k = h_pipe_jp1_k::read();
        auto h_j_km1 = h_pipe_j_km1::read();
        auto h_jm1_k = h_pipe_jm1_k::read();
        auto vn_j_k = vn_pipe_j_k::read();
        auto vn_j_km1 = vn_pipe_j_km1::read();
        auto vn_jm1_k = vn_pipe_jm1_k::read();
        auto un_j_k = un_pipe_j_k::read();
        auto un_j_km1 = un_pipe_j_km1::read();
        auto un_jm1_k = un_pipe_jm1_k::read();

        auto eta_j_k = eta_pipe_j_k_2::read();

        float hep___dyn = 0.5 * (un_j_k + (float)fabs(un_j_k)) * h_j_k;
        float hen___dyn = 0.5 * (un_j_k - (float)fabs(un_j_k)) * h_j_kp1;
        float hue___dyn = hep___dyn + hen___dyn;
        float hwp___dyn = 0.5 * (un_j_km1 + (float)fabs(un_j_km1)) * h_j_km1;
        float hwn___dyn = 0.5 * (un_j_km1 - (float)fabs(un_j_km1)) * h_j_k;
        float huw___dyn = hwp___dyn + hwn___dyn;
        float hnp___dyn = 0.5 * (vn_j_k + (float)fabs(vn_j_k)) * h_j_k;
        float hnn___dyn = 0.5 * (vn_j_k - (float)fabs(vn_j_k)) * h_jp1_k;
        float hvn___dyn = hnp___dyn + hnn___dyn;
        float hsp___dyn = 0.5 * (vn_jm1_k + (float)fabs(vn_jm1_k)) * h_jm1_k;
        float hsn___dyn = 0.5 * (vn_jm1_k - (float)fabs(vn_jm1_k)) * h_j_k;
        float hvs___dyn = hsp___dyn + hsn___dyn;
        float etan_j_k =
            eta_j_k - dt * (hue___dyn - huw___dyn) / dx - dt * (hvn___dyn - hvs___dyn) / dy;

        etan_pipe::write(etan_j_k);
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class wet_2_smache>([=]() {
      constexpr uint REACH_POS = std::max(OFFSET_J_KP1, OFFSET_JP1_K);
      constexpr uint REACH_NEG = std::max(OFFSET_J_KM1, OFFSET_JM1_K);
      constexpr uint STENCIL_REACH = REACH_NEG + REACH_POS;

      // Store the stencil reach + the current i_j_k element
      float wet_buffer[STENCIL_REACH + 1];

      for (int count = 0; count < (ARRAY_SIZE + STENCIL_REACH); count++) {
// SHIFT-RIGHT register for the buffers
// TODO: Probably no need for this pragma.
#pragma unroll
        for (int i = 0; i < STENCIL_REACH; ++i) {
          wet_buffer[i] = wet_buffer[i + 1];
        }

        if (count < DOMAIN_SIZE) {
          wet_buffer[STENCIL_REACH] = wet_pipe_2::read();
        }

        // start emitting once all data covered by the stencil reach is read into the buffer
        constexpr uint IDX_J_K = REACH_NEG;
        if (count >= STENCIL_REACH) {
          wet_pipe_2_j_k::write(wet_buffer[IDX_J_K]);
          wet_pipe_2_j_kp1::write(wet_buffer[IDX_J_K + OFFSET_J_KP1]);
          wet_pipe_2_jp1_k::write(wet_buffer[IDX_J_K + OFFSET_JP1_K]);
          wet_pipe_2_j_km1::write(wet_buffer[IDX_J_K - OFFSET_J_KM1]);
          wet_pipe_2_jm1_k::write(wet_buffer[IDX_J_K - OFFSET_JM1_K]);
        }
      }
    });
  });

  q.submit([&](handler &hnd) {
    hnd.single_task<class etan_smache>([=]() {
      constexpr uint REACH_POS = std::max(OFFSET_J_KP1, OFFSET_JP1_K);
      constexpr uint REACH_NEG = std::max(OFFSET_J_KM1, OFFSET_JM1_K);
      constexpr uint STENCIL_REACH = REACH_NEG + REACH_POS;

      // Store the stencil reach + the current i_j_k element
      float etan_buffer[STENCIL_REACH + 1];

      for (int count = 0; count < (ARRAY_SIZE + STENCIL_REACH); count++) {
// SHIFT-RIGHT register for the buffers
// TODO: Probably no need for this pragma.
#pragma unroll
        for (int i = 0; i < STENCIL_REACH; ++i) {
          etan_buffer[i] = etan_buffer[i + 1];
        }

        if (count < DOMAIN_SIZE) {
          etan_buffer[STENCIL_REACH] = etan_pipe::read();
        }

        // start emitting once all data covered by the stencil reach is read into the buffer
        constexpr uint IDX_J_K = REACH_NEG;
        if (count >= STENCIL_REACH) {
          etan_pipe_j_k::write(etan_buffer[IDX_J_K]);
          etan_pipe_j_kp1::write(etan_buffer[IDX_J_K + OFFSET_J_KP1]);
          etan_pipe_jp1_k::write(etan_buffer[IDX_J_K + OFFSET_JP1_K]);
          etan_pipe_j_km1::write(etan_buffer[IDX_J_K - OFFSET_J_KM1]);
          etan_pipe_jm1_k::write(etan_buffer[IDX_J_K - OFFSET_JM1_K]);
        }
      }
    });
  });

  q.submit([&](handler &hnd) {
    accessor etann(etann_buf, hnd, write_only, no_init);

    hnd.single_task<class map92>([=]() {
      for (int global_id = 0; global_id < ARRAY_SIZE; ++global_id) {
        auto wet_j_k = wet_pipe_2_j_k::read();
        auto wet_j_kp1 = wet_pipe_2_j_kp1::read();
        auto wet_jp1_k = wet_pipe_2_jp1_k::read();
        auto wet_j_km1 = wet_pipe_2_j_km1::read();
        auto wet_jm1_k = wet_pipe_2_jm1_k::read();
        auto etan_j_k = etan_pipe_j_k::read();
        auto etan_j_kp1 = etan_pipe_j_kp1::read();
        auto etan_jp1_k = etan_pipe_jp1_k::read();
        auto etan_j_km1 = etan_pipe_j_km1::read();
        auto etan_jm1_k = etan_pipe_jm1_k::read();

        float etann_j_k = 0.0;
        if (wet_j_k == 1) {
          float term1___map75 =
              float(1.0 - 0.25 * eps * (wet_j_kp1 + wet_j_km1 + wet_jp1_k + wet_jm1_k)) * etan_j_k;
          float term2___map75 = 0.25 * eps * (wet_j_kp1 * etan_j_kp1 + wet_j_km1 * etan_j_km1);
          float term3___map75 = 0.25 * eps * (wet_jp1_k * etan_jp1_k + wet_jm1_k * etan_jm1_k);
          etann_j_k = term1___map75 + term2___map75 + term3___map75;
        } else {
          etann_j_k = etan_j_k;
        }

        etann[global_id] = etann_j_k;
      }
    });
  });
}
