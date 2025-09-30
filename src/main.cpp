#include <iostream>
#include <cmath>
#include <algorithm>
#include <map>
#include <list>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdint>
#include <format>
#include <iterator>
#include <numeric>
#include <sstream>

#define GRAVITY = 9.8f;
#ifdef NDEBUG
#endif

enum class OrderType
{
  Invalid = -1,
  GoodTillCancel = 1,
  FillAndKill
};

enum class Side
{
  Invalid = -1,
  Buy = 1,
  Sell
};

using Price = std::uint32_t;
using Quantity = std::uint32_t;

struct LevelInfo
{
  Price m_price;
  Quantity m_quantity;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos
{
public:
  OrderBookLevelInfos(const LevelInfos &bids, const LevelInfos &asks)
      : m_bids{bids}, m_asks{asks}
  {
  }

  const LevelInfos &GetBids() const { return m_bids; }
  const LevelInfos &GetAsks() const { return m_asks; }

private:
  LevelInfos m_bids;
  LevelInfos m_asks;
};

class Order
{
public:
  Order(OrderType orderType, std::uint64_t orderId, Side side, Price price, Quantity quantity)
      : m_orderType{orderType},
        m_orderId{orderId},
        m_side{side},
        m_price{price},
        m_initialQuantity{quantity},
        m_remainingQuantity{quantity}
  {
  }

  OrderType GetOrderType() const { return m_orderType; }
  std::uint64_t GetOrderId() const { return m_orderId; }
  Side GetSide() const { return m_side; }
  Price GetPrice() const { return m_price; }
  Quantity GetInitialQuantity() const { return m_initialQuantity; }
  Quantity GetRemainingQuantity() const { return m_remainingQuantity; }
  Quantity GetFilledQuantity() const { return m_initialQuantity - m_remainingQuantity; }
  void Fill(Quantity quantity)
  {
    if (quantity > m_remainingQuantity)
      throw std::logic_error(std::format("Order ({}) cannot be filled for more than it's remaining quantity", m_orderId));
    m_remainingQuantity -= quantity;
  }

  bool IsFilled() const
  {
    return m_remainingQuantity == 0;
  }

private:
  OrderType m_orderType;
  std::uint64_t m_orderId;
  Side m_side;
  Price m_price;
  Quantity m_initialQuantity;
  Quantity m_remainingQuantity;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify
{
public:
  OrderModify(std::uint64_t orderId, Side side, Price price, Quantity quantity)
      : m_orderId{orderId},
        m_side{side},
        m_price{price},
        m_quantity{quantity}
  {
  }
  std::uint64_t GetOrderId() const { return m_orderId; }
  Side GetSide() const { return m_side; }
  Price GetPrice() const { return m_price; }
  Quantity GetQuantity() const { return m_quantity; }

  OrderPointer ToOrderPointer(OrderType type) const
  {
    return std::make_shared<Order>(type, m_orderId, m_side, m_price, m_quantity);
  }

private:
  std::uint64_t m_orderId;
  Side m_side;
  Price m_price;
  Quantity m_quantity;
};

struct TradeInfo
{
  std::uint64_t m_orderInfo;
  Price m_price;
  Quantity m_quantity;
};

class Trade
{
public:
  Trade(const TradeInfo &bidTrade, const TradeInfo &askTrade)
      : m_bidTrade{bidTrade},
        m_askTrade{askTrade}
  {
  }
  TradeInfo GetBidTrade() const { return m_bidTrade; }
  TradeInfo GetAskTrade() const { return m_askTrade; }

private:
  TradeInfo m_bidTrade;
  TradeInfo m_askTrade;
};

using Trades = std::vector<Trade>;

class OrderBook
{
public:
  Trades AddOrder(OrderPointer order)
  {
    if (m_orders.contains(order->GetOrderId()))
      return {};
    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
      return {};
    OrderPointers::iterator iterator;

    if (order->GetSide() == Side::Buy)
    {
      auto &orders = m_bids[order->GetPrice()];
      orders.push_back(order);
      iterator = std::next(orders.begin(), orders.size() - 1);
    }
    else
    {
      auto &orders = m_asks[order->GetPrice()];
      orders.push_back(order);
      iterator = std::next(orders.begin(), orders.size() - 1);
    }
    m_orders.insert({order->GetOrderId(), OrderEntry{order, iterator}});
    return MatchOrder();
  }

  void CancelOrder(std::uint64_t orderId)
  {
    if (!m_orders.contains(orderId))
    {
      std::cout << "Order not found!" << std::endl;
      return;
    }
    const auto &[order, iterator] = m_orders.at(orderId);
    m_orders.erase(orderId);

    if (order->GetSide() == Side::Sell)
    {
      auto price = order->GetPrice();
      auto &orders = m_asks.at(price);
      orders.erase(iterator);
      if (orders.empty())
        m_asks.erase(price);
    }
    else
    {
      auto price = order->GetPrice();
      auto &orders = m_bids.at(price);
      orders.erase(iterator);
      if (orders.empty())
        m_bids.erase(price);
    }
  }

  Trades MatchOrder(OrderModify order)
  {
    if (!m_orders.contains(order.GetOrderId()))
      return {};
    const auto &[existingOrder, _] = m_orders.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
  }

  std::size_t Size() const { return m_orders.size(); }

  OrderBookLevelInfos GetOrderInfos() const
  {
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(m_orders.size());
    askInfos.reserve(m_orders.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers &orders)
    {
      return LevelInfo{
          price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                                 [](std::size_t runningSum, const OrderPointer &order)
                                 { return runningSum + order->GetRemainingQuantity(); })};
    };

    for (const auto &[price, orders] : m_bids)
    {
      bidInfos.push_back(CreateLevelInfos(price, orders));
    }
    for (const auto &[price, orders] : m_asks)
    {
      askInfos.push_back(CreateLevelInfos(price, orders));
    }

    return OrderBookLevelInfos{bidInfos, askInfos};
  }

  std::vector<OrderModify> GetOrderLists()
  {
    std::vector<OrderModify> OrderModifies;
    for (auto &&[id, order] : m_orders)
    {
      OrderPointer orderPointer = order.m_order;
      OrderModifies.push_back(OrderModify{id, orderPointer->GetSide(), orderPointer->GetPrice(), orderPointer->GetInitialQuantity()});
    }
    return OrderModifies;
  }

private:
  struct OrderEntry
  {
    OrderPointer m_order{nullptr};
    OrderPointers::iterator m_location;
  };

  std::map<Price, OrderPointers, std::greater<Price>> m_bids;
  std::map<Price, OrderPointers, std::less<Price>> m_asks;
  std::unordered_map<std::uint64_t, OrderEntry> m_orders;

  bool CanMatch(Side side, Price price) const
  {
    if (side == Side::Buy)
    {
      if (m_asks.empty())
        return false;
      const auto &[bestAsk, _] = *m_asks.begin();
      return price >= bestAsk;
    }
    else
    {
      if (m_bids.empty())
        return false;
      const auto &[bestBid, _] = *m_bids.begin();
      return price <= bestBid;
    }
  }

  Trades MatchOrder()
  {
    Trades trades;
    trades.reserve(m_orders.size());

    while (true)
    {
      if (m_bids.empty() || m_asks.empty())
        break;
      auto &[bidPrice, bids] = *m_bids.begin();
      auto &[askPrice, asks] = *m_asks.begin();

      if (bidPrice < askPrice)
        break;
      while (bids.size() && asks.size())
      {
        auto &bid = bids.front();
        auto &ask = asks.front();

        Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

        bid->Fill(quantity);
        ask->Fill(quantity);

        if (bid->IsFilled())
        {
          bids.pop_front();
          m_orders.erase(bid->GetOrderId());
        }

        if (ask->IsFilled())
        {
          asks.pop_front();
          m_orders.erase(ask->GetOrderId());
        }

        if (bids.empty())
        {
          m_bids.erase(bidPrice);
        }

        if (asks.empty())
        {
          m_asks.erase(askPrice);
        }

        trades.push_back(Trade{
            TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
            TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}});
      }
    }
    if (!m_bids.empty())
    {
      auto &[_, bids] = *m_bids.begin();
      auto &order = bids.front();
      if (order->GetOrderType() == OrderType::FillAndKill)
      {
        CancelOrder(order->GetOrderId());
      }
    }
    if (!m_asks.empty())
    {
      auto &[_, asks] = *m_asks.begin();
      auto &order = asks.front();
      if (order->GetOrderType() == OrderType::FillAndKill)
      {
        CancelOrder(order->GetOrderId());
      }
    }

    return trades;
  }
};

void ClearScreen()
{
#if _WIN32
  system("cls");
#else
  system("clear");
#endif
}

OrderType ToOrderType(int value)
{
  switch (value)
  {
  case 1:
    return OrderType::GoodTillCancel;
  case 2:
    return OrderType::FillAndKill;
  default:
    return OrderType::Invalid;
  }
}

Side ToSide(int value)
{
  switch (value)
  {
  case 1:
    return Side::Buy;
  case 2:
    return Side::Sell;
  default:
    return Side::Invalid;
  }
}

enum class Command
{
  Invalid = -1,
  EnterOrder = 1,
  CancelOrder,
  DisplayOrderList,
  DisplayOrderbook,
  Exit
};

int main()
{

  ClearScreen();
  OrderBook orderBook;
  std::uint64_t orderId = 1;

  orderBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 50));
  orderBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 200, 50));

  int input;
  std::string stream;

  while (true)
  {
    std::cout << "Enter a command: " << std::endl;
    std::cout << "1) Enter an order" << std::endl;
    std::cout << "2) Cancel an order" << std::endl;
    std::cout << "3) Display entry history" << std::endl;
    std::cout << "4) Display orderbook" << std::endl;
    std::cout << "5) Exit" << std::endl;

    std::getline(std::cin, stream);
    std::istringstream buffer(stream);

    if (!(buffer >> input))
    {
      ClearScreen();
      std::cerr << "Invalid Input" << std::endl;
      continue;
    }

    ClearScreen();

    Command command = static_cast<Command>(input);
    if (command == Command::Exit)
    {
      break;
    }

    switch (command)
    {
    case Command::EnterOrder:
    {
      std::cout << "Enter orderType, side, price, quantity: " << std::endl;
      std::cout << "OrderType: 1) GoodTillCancel, Side: 1) Buy" << std::endl;
      std::cout << "           2) FillAndKill,          2) Sell" << std::endl;
      std::cout << "ex. 1(GoodTillCancel), 1(Buy), 50, 5" << std::endl;

      int type, side, price, quantity;
      std::getline(std::cin, stream);
      std::istringstream buffer(stream);
      buffer >> type >> side >> price >> quantity;

      if (ToOrderType(type) == OrderType::Invalid || ToSide(side) == Side::Invalid)
      {
        std::cout << "Invalid Input" << std::endl;
        break;
      }
      orderBook.AddOrder(std::make_shared<Order>(ToOrderType(type), orderId, ToSide(side), price, quantity));
      orderId++;
      ClearScreen();
      std::cout << "Added!" << std::endl;
    }
    break;
    case Command::CancelOrder:
    {
      std::cout << "Enter order id your want to cancel:" << std::endl;
      std::uint64_t cancelId;
      std::getline(std::cin, stream);
      std::istringstream buffer(stream);
      buffer >> cancelId;
      orderBook.CancelOrder(cancelId);
      ClearScreen();
      std::cout << "Canceled!" << std::endl;
    }
    break;
    case Command::DisplayOrderList:
    {
      std::vector<OrderModify> orderList = orderBook.GetOrderLists();
      if (orderList.empty())
      {
        std::cout << "No entry have been entered yet!" << std::endl;
        std::cout << "Press enter to continue" << std::endl;
        std::cin.get();
        ClearScreen();
        break;
      }
      auto sideToStr = [](Side side)
      { return static_cast<int>(side) == 1 ? "Buy" : "Sell"; };

      for (auto &&iterator = orderList.begin(); iterator != orderList.end(); iterator++)
      {
        std::cout << "Id: " << (*iterator).GetOrderId() << " Side: " << sideToStr((*iterator).GetSide()) << " Price: " << (*iterator).GetPrice() << " Quantity: " << (*iterator).GetQuantity() << std::endl;
      }
      std::cout << "Press enter to continue" << std::endl;
      std::cin.get();
      ClearScreen();
    }
    break;
    case Command::DisplayOrderbook:
    {
      OrderBookLevelInfos levelInfos = orderBook.GetOrderInfos();
      LevelInfos bids = levelInfos.GetBids();
      LevelInfos asks = levelInfos.GetAsks();
      std::cout << "Buy:" << std::endl;
      for (auto &&info : bids)
      {
        std::cout << "\x1b[32mPrice: " << info.m_price << " Quantity: " << info.m_quantity << "\x1b[m" << std::endl;
      }
      std::cout << "Sell:" << std::endl;
      for (auto &&info : asks)
      {
        std::cout << "\x1b[31mPrice: " << info.m_price << " Quantity: " << info.m_quantity << "\x1b[m" << std::endl;
      }
      std::cout << "Press enter to continue" << std::endl;
      std::cin.get();
      ClearScreen();
    }
    break;
    default:
      std::cout << "Invalid Command!" << std::endl;
      break;
    }
  }

  return 0;
}
