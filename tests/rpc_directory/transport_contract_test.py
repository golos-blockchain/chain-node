"""Source-contract checks, NOT a substitute for a full network build/test."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]
class TransportContracts(unittest.TestCase):
    def test_admission_before_extension_delivery(self):
        src = (ROOT / 'libraries/network/node.cpp').read_text()
        self.assertIn('peer->pending_hello_extension', src)
        body = src.split('void node_impl::move_peer_to_active_list', 1)[1].split('void node_impl::move_peer_to_closing_list', 1)[0]
        self.assertIn('_hello_consumer', body)
        self.assertIn('their_connection_state::connection_accepted', body)
        self.assertIn('our_connection_state::connection_accepted', body)
        self.assertIn('STEEMIT_CHAIN_ID', body)
    def test_plugin_and_client_are_explicit_and_linked(self):
        cmake = (ROOT / 'plugins/CMakeLists.txt').read_text()
        self.assertTrue('BUILD_RPC_DIRECTORY' in cmake, 'optional default-OFF build gate missing')
        self.assertTrue((ROOT / 'programs/rpc_directory_client/main.cpp').exists(), 'no standalone client')
        self.assertTrue((ROOT / 'plugins/rpc_directory/rpc_directory_plugin.cpp').exists(), 'no plugin')
    def test_send_boundary_filters_virtual_and_real_messages(self):
        src = (ROOT / 'libraries/network/peer_connection.cpp').read_text()
        self.assertTrue('discovery_control_message(message_to_send.msg_type)' in src, 'missing final send guard')
        self.assertTrue('discovery_control_message(item_to_send.item_type)' in src, 'missing virtual-item guard')
    def test_plugin_catalog_is_lazy_and_read_before_initialize_is_safe(self):
        src = (ROOT / 'plugins/rpc_directory/rpc_directory_plugin.cpp').read_text()
        constructor = src.split('plugin::plugin()', 1)[1].split('void plugin::set_program_options', 1)[0]
        initialize = src.split('void plugin::plugin_initialize', 1)[1].split('void plugin::plugin_shutdown', 1)[0]
        getter = src.split('plugin::get_directory() const', 1)[1]
        self.assertNotIn('make_shared', constructor, 'disabled plugin eagerly allocates catalog')
        self.assertIn('catalog_ = std::make_shared<network::rpc_directory::catalog>()', initialize)
        self.assertIn('if (!catalog_) return {};', getter, 'read before initialize must be empty')
    def test_opt_in_no_chain_loops(self):
        src = (ROOT / 'libraries/network/node.cpp').read_text()
        self.assertIn('if (_discovery_only && !discovery_control_message(received_message.msg_type))', src)
        self.assertIn('if (!_discovery_only) {\n                _fetch_sync_items_loop_done', src)
        self.assertIn('if (_discovery_only || peer->discovery_only)', src.split('void node_impl::start_synchronizing_with_peer', 1)[1][:350])
if __name__ == '__main__': unittest.main()
