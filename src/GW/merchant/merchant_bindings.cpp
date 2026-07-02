#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "GW/merchant/merchant.h"

#include <vector>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyMerchant, m) {
    m.doc() = "Py4GW Merchant bindings";

    m.def("transact_items", [](uint32_t type, uint32_t gold_give, const py::list& give_item_ids, const py::list& give_quantities,
                                uint32_t gold_recv, const py::list& recv_item_ids, const py::list& recv_quantities) -> bool {
        auto txn_type = static_cast<GW::Constants::TransactionType>(type);

        size_t give_count = std::min(give_item_ids.size(), give_quantities.size());
        std::vector<uint32_t> g_ids(give_count), g_qties(give_count);
        for (size_t i = 0; i < give_count; ++i) {
            g_ids[i] = give_item_ids[i].cast<uint32_t>();
            g_qties[i] = give_quantities[i].cast<uint32_t>();
        }
        GW::Context::MerchantTransactionInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_count);
        give_info.item_ids = give_count > 0 ? g_ids.data() : nullptr;
        give_info.item_quantities = give_count > 0 ? g_qties.data() : nullptr;

        size_t recv_count = std::min(recv_item_ids.size(), recv_quantities.size());
        std::vector<uint32_t> r_ids(recv_count), r_qties(recv_count);
        for (size_t i = 0; i < recv_count; ++i) {
            r_ids[i] = recv_item_ids[i].cast<uint32_t>();
            r_qties[i] = recv_quantities[i].cast<uint32_t>();
        }
        GW::Context::MerchantTransactionInfo recv_info;
        recv_info.item_count = static_cast<uint32_t>(recv_count);
        recv_info.item_ids = recv_count > 0 ? r_ids.data() : nullptr;
        recv_info.item_quantities = recv_count > 0 ? r_qties.data() : nullptr;

        return GW::merchant::TransactItems(txn_type, gold_give, give_info, gold_recv, recv_info);
    },
    py::arg("type"), py::arg("gold_give") = 0, py::arg("give_item_ids") = py::list(),
    py::arg("give_quantities") = py::list(), py::arg("gold_recv") = 0,
    py::arg("recv_item_ids") = py::list(), py::arg("recv_quantities") = py::list());

    m.def("transact_item_simple", [](uint32_t type, uint32_t item_id, uint32_t quantity, uint32_t gold) -> bool {
        auto txn_type = static_cast<GW::Constants::TransactionType>(type);
        uint32_t item_ids = item_id;
        uint32_t quantities = quantity;
        GW::Context::MerchantTransactionInfo txn_info;
        if (item_id != 0) {
            txn_info.item_count = 1;
            txn_info.item_ids = &item_ids;
            txn_info.item_quantities = &quantities;
        }
        GW::Context::MerchantTransactionInfo empty_info = {};
        return GW::merchant::TransactItems(txn_type, gold, txn_info, 0, empty_info);
    }, py::arg("type"), py::arg("item_id"), py::arg("quantity"), py::arg("gold") = 0);

    m.def("request_quote", [](uint32_t type, const py::list& give_item_ids, const py::list& recv_item_ids) -> bool {
        auto quote_type = static_cast<GW::Constants::TransactionType>(type);
        size_t give_count = give_item_ids.size();
        std::vector<uint32_t> g_ids(give_count);
        for (size_t i = 0; i < give_count; ++i)
            g_ids[i] = give_item_ids[i].cast<uint32_t>();
        GW::Context::MerchantQuoteInfo give_info;
        give_info.item_count = static_cast<uint32_t>(give_count);
        give_info.item_ids = give_count > 0 ? g_ids.data() : nullptr;

        size_t recv_count = recv_item_ids.size();
        std::vector<uint32_t> r_ids(recv_count);
        for (size_t i = 0; i < recv_count; ++i)
            r_ids[i] = recv_item_ids[i].cast<uint32_t>();
        GW::Context::MerchantQuoteInfo recv_info;
        recv_info.item_count = static_cast<uint32_t>(recv_count);
        recv_info.item_ids = recv_count > 0 ? r_ids.data() : nullptr;

        return GW::merchant::RequestQuote(quote_type, give_info, recv_info);
    }, py::arg("type"), py::arg("give_item_ids") = py::list(), py::arg("recv_item_ids") = py::list());

    m.def("request_quote_simple", [](uint32_t type, uint32_t item_id) -> bool {
        auto quote_type = static_cast<GW::Constants::TransactionType>(type);
        uint32_t item_ids = item_id;
        GW::Context::MerchantQuoteInfo quote_info;
        if (item_id != 0) {
            quote_info.item_count = 1;
            quote_info.item_ids = &item_ids;
        }
        GW::Context::MerchantQuoteInfo empty_info = {};
        return GW::merchant::RequestQuote(quote_type, quote_info, empty_info);
    }, py::arg("type"), py::arg("item_id"));
}
