#include <lgr_exodus.hpp>
#include <lgr_state.hpp>
#include <lgr_pinned_vector.hpp>
#include <lgr_meshing.hpp>
#include <lgr_input.hpp>
#include <lgr_copy.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#include <exodusII.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace lgr {

void read_exodus_file(std::string const& filepath, input const& in, state& s) {
  int comp_ws = int(sizeof(double));
  int io_ws = 0;
  float version;
  auto mode = EX_READ;
  int exodus_file = ex_open(filepath.c_str(), mode, &comp_ws, &io_ws, &version);
  assert(exodus_file >= 0);
  ex_init_params init_params;
  int exodus_error_code;
  exodus_error_code = ex_get_init_ext(exodus_file, &init_params);
  assert(exodus_error_code == 0);
  host_vector<int> block_ids(int(init_params.num_elem_blk));
  exodus_error_code = ex_get_ids(exodus_file, EX_ELEM_BLOCK, block_ids.data());
  assert(exodus_error_code == 0);
  switch (in.element) {
    case BAR:
      s.nodes_in_element.resize(node_in_element_index(2));
      s.points_in_element.resize(point_in_element_index(1));
      break;
    case TRIANGLE:
      s.nodes_in_element.resize(node_in_element_index(3));
      s.points_in_element.resize(point_in_element_index(1));
      break;
    case TETRAHEDRON:
      s.nodes_in_element.resize(node_in_element_index(4));
      s.points_in_element.resize(point_in_element_index(1));
      break;
    case COMPOSITE_TETRAHEDRON:
      s.nodes_in_element.resize(node_in_element_index(10));
      s.points_in_element.resize(point_in_element_index(4));
      break;
  }
  s.nodes.resize(node_index(int(init_params.num_nodes)));
  s.elements.resize(element_index(int(init_params.num_elem)));
  s.material.resize(s.elements.size());
  host_vector<int> host_conn(int(s.elements.size() * s.nodes_in_element.size()));
  int offset = 0;
  for (int i = 0; i < init_params.num_elem_blk; ++i) {
    char elem_type[MAX_STR_LENGTH + 1];
    int nentries;
    int nnodes_per_entry;
    int nedges_per_entry;
    int nfaces_per_entry;
    int nattr_per_entry;
    exodus_error_code = ex_get_block(exodus_file, EX_ELEM_BLOCK, block_ids[i], elem_type,
        &nentries, &nnodes_per_entry, &nedges_per_entry, &nfaces_per_entry, &nattr_per_entry);
    assert(exodus_error_code == 0);
    if (nentries == 0) continue;
    assert(nnodes_per_entry == int(s.nodes_in_element.size()));
    if (nedges_per_entry < 0) nedges_per_entry = 0;
    if (nfaces_per_entry < 0) nfaces_per_entry = 0;
    host_vector<int> edge_conn(nentries * nedges_per_entry);
    host_vector<int> face_conn(nentries * nfaces_per_entry);
    exodus_error_code = ex_get_conn(exodus_file, EX_ELEM_BLOCK, block_ids[i],
        host_conn.data() + offset * int(s.nodes_in_element.size()), edge_conn.data(),
        face_conn.data());
    assert(exodus_error_code == 0);
    auto material_begin = s.material.begin() + element_index(offset);
    auto material_end = material_begin + element_index(nentries);
    iterator_range<decltype(material_begin)> material_range(material_begin, material_end);
    material_index const material(block_ids[i]);
    fill(material_range, material);
    offset += nentries;
  }
  assert(offset == init_params.num_elem);
  pinned_vector<node_index, element_node_index> pinned_conn(
      s.elements.size() * s.nodes_in_element.size(), s.pinpool);
  auto const elements_to_element_nodes = s.elements * s.nodes_in_element;
  for (auto const element : s.elements) {
    auto const element_nodes = elements_to_element_nodes[element];
    for (auto const node_in_element : s.nodes_in_element) {
      int const host_index = int(element * s.nodes_in_element.size()) + int(node_in_element);
      auto const element_node = element_nodes[node_in_element];
      pinned_conn[element_node] = node_index(host_conn[host_index] - 1);
    }
  }
  s.elements_to_nodes.resize(pinned_conn.size());
  copy(pinned_conn, s.elements_to_nodes);
  pinned_conn.clear();
  host_vector<double, node_index> host_coords[3];
  for (int i = 0; i < 3; ++i) host_coords[i].resize(s.nodes.size());
  exodus_error_code = ex_get_coord(exodus_file,
      host_coords[0].data(),
      host_coords[1].data(),
      host_coords[2].data());
  assert(exodus_error_code == 0);
  pinned_vector<vector3<double>, node_index> pinned_coords(s.nodes.size(), s.pinpool);
  for (auto const node : s.nodes) {
    pinned_coords[node] = vector3<double>(
        host_coords[0][node],
        host_coords[1][node],
        host_coords[2][node]);
  }
  s.x.resize(s.nodes.size());
  copy(pinned_coords, s.x);
  propagate_connectivity(s);
}

}