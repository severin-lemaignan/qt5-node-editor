/* See LICENSE file for copyright and license details. */

#include "graphicsnode.hpp"
#include <QPushButton>
#include <QPen>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsProxyWidget>
#include <QLineEdit>
#include <QLabel>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsProxyWidget>
#include <QDebug>

#include <algorithm>
#include <iostream>
#include <tuple>
#include <memory>

#include "editablelabel.hpp"

#include "graphicsbezieredge.hpp"
#include "graphicsnodesocket.hpp"

using namespace std;

GraphicsNode::GraphicsNode(NodePtr node, QGraphicsItem *parent)
    : QGraphicsItem(parent)
        , _node(node)
        , _changed(false)
        , _width(150)
        , _height(120)
        , _pen_default(QColor("#7F000000"))
        , _pen_selected(QColor("#FFFF36A7"))
        // , _pen_selected(QColor("#FFFFA836"))
        // , _pen_selected(QColor("#FF00FF27"))
        , _pen_sources(QColor("#FF000000"))
        , _pen_sinks(QColor("#FF000000"))
        , _brush_title(QColor("#E3212121"))
        , _brush_background(QColor("#E31a1a1a"))
        , _brush_sources(QColor("#FFFF7700"))
        , _brush_sinks(QColor("#FF0077FF"))
        , _effect(new QGraphicsDropShadowEffect())
        , _title_item(new EditableLabel(this))
{
    for (auto p : {&_pen_default, &_pen_selected, &_pen_default, &_pen_selected}) {
        p->setWidth(0);
    }

    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);

    _title_item->setPos(0, 0);
    _title_item->setTextWidth(_width - 2*_lr_padding);
    QObject::connect(_title_item, &EditableLabel::contentUpdated,
            this, &GraphicsNode::updateNode);

    // alignment?
    /*
       auto opts = q->document()->defaultTextOption();
       opts.setAlignment(Qt::AlignRight);
       q->document()->setDefaultTextOption(opts);
       */

    _effect->setBlurRadius(13.0);
    _effect->setColor(QColor("#99121212"));
    setGraphicsEffect(_effect);

    refreshNode();

}


void GraphicsNode::
setTitle(const QString &title)
{
    _title = title;
    _title_item->setPlainText(title);
}


GraphicsNode::
~GraphicsNode()
{
    qWarning() << "Widget for Node " << QString::fromStdString(_node->name()) << " deleted";
    if (_central_proxy) delete _central_proxy;
    delete _title_item;
    delete _effect;
}


QRectF GraphicsNode::
boundingRect() const
{
    return QRectF(-_pen_width/2.0 - _socket_size,
            -_pen_width/2.0,
            _width + _pen_width/2.0 + 2.0 * _socket_size,
            _height + _pen_width/2.0).normalized();
}


void GraphicsNode::
paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const qreal edge_size = 10.0;
    const qreal title_height = 20.0;

    // path for the caption of this node
    QPainterPath path_title;
    path_title.setFillRule(Qt::WindingFill);
    path_title.addRoundedRect(QRect(0, 0, _width, title_height), edge_size, edge_size);
    path_title.addRect(0, title_height - edge_size, edge_size, edge_size);
    path_title.addRect(_width - edge_size, title_height - edge_size, edge_size, edge_size);
    painter->setPen(Qt::NoPen);
    painter->setBrush(_brush_title);
    painter->drawPath(path_title.simplified());

    // path for the content of this node
    QPainterPath path_content;
    path_content.setFillRule(Qt::WindingFill);
    path_content.addRoundedRect(QRect(0, title_height, _width, _height - title_height), edge_size, edge_size);
    path_content.addRect(0, title_height, edge_size, edge_size);
    path_content.addRect(_width - edge_size, title_height, edge_size, edge_size);
    painter->setPen(Qt::NoPen);
    painter->setBrush(_brush_background);
    painter->drawPath(path_content.simplified());

    // path for the outline
    QPainterPath path_outline = QPainterPath();
    path_outline.addRoundedRect(QRect(0, 0, _width, _height), edge_size, edge_size);
    painter->setPen(isSelected() ? _pen_selected : _pen_default);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path_outline.simplified());

    // debug bounding box
#if 0
    QPen debugPen = QPen(QColor(Qt::red));
    debugPen.setWidth(0);
    auto r = boundingRect();
    painter->setPen(debugPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(r);

    painter->drawPoint(0,0);
#endif
}


void GraphicsNode::
mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // TODO: ordering after selection/deselection cycle
    QGraphicsItem::mousePressEvent(event);
    if (isSelected())
        setZValue(1);
    else
        setZValue(0);
}


void GraphicsNode::
setSize(const qreal width, const qreal height)
{
    setSize(QPointF(width, height));
}


void GraphicsNode::
setSize(const QPointF size)
{
    setSize(QSizeF(size.x(), size.y()));
}


void GraphicsNode::
setSize(const QSizeF size)
{
    _width = size.width();
    _height = size.height();
    _changed = true;
    prepareGeometryChange();
    updateGeometry();
}


QVariant GraphicsNode::
itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) {
        case QGraphicsItem::ItemSelectedChange: {
                                                    if (value == true)
                                                        setZValue(1);
                                                    else
                                                        setZValue(0);
                                                    break;
                                                }
        case QGraphicsItem::ItemPositionChange:
        case QGraphicsItem::ItemPositionHasChanged:
                                                propagateChanges();
                                                break;

        default:
                                                break;
    }

    return QGraphicsItem::itemChange(change, value);
}


const GraphicsNodeSocket* GraphicsNode::
add_socket(shared_ptr<Port> port)
{

    GraphicsNodeSocket* s;

    if (port->direction == Port::Direction::IN) {
        s = new GraphicsNodeSocket(port, this);
        _sinks.push_back(s);
    }
    else {
        s = new GraphicsNodeSocket(port, this);
        _sources.push_back(s);
    }

    _changed = true;
    prepareGeometryChange();
    updateGeometry();
    return s;
}

GraphicsNodeSocket* GraphicsNode::
connect_source(ConstPortPtr port, GraphicsDirectedEdge *edge)
{
    auto source = find_if(_sources.begin(), _sources.end(), 
                          [&port](const GraphicsNodeSocket* socket) { 
                               return socket->port() == port; 
                          });

    if (source == _sources.end()) {
        qWarning() << "Trying to connect source " << QString::fromStdString(port->name) << " of node " << _title << ", but it does not exist.";
        return nullptr;
    }
    (*source)->set_edge(edge);

    return *source;
}


GraphicsNodeSocket* GraphicsNode::
connect_sink(ConstPortPtr port, GraphicsDirectedEdge *edge)
{
    auto sink = find_if(_sinks.begin(), _sinks.end(), 
                          [&port](const GraphicsNodeSocket* socket) { 
                               return socket->port() == port; 
                          });

    if (sink == _sinks.end()) {
        qWarning() << "Trying to connect sink " << QString::fromStdString(port->name) << " of node " << _title << ", but it does not exist.";
        return nullptr;
    }
    (*sink)->set_edge(edge);

    return *sink;
}

void GraphicsNode::
refreshNode()
{
    setTitle(QString::fromStdString(_node->name()));


    set<PortPtr> in_node = _node->ports();
    set<PortPtr> existing;
    set<PortPtr> to_add;
    set<PortPtr> to_remove;

    for(auto s : _sinks) {
        existing.insert(s->port());
    }
    for(auto s : _sources) {
        existing.insert(s->port());
    }

    set_difference(in_node.begin(), in_node.end(), 
                   existing.begin(), existing.end(), 
                   inserter(to_add, to_add.begin()));

    set_difference(existing.begin(), existing.end(),
                   in_node.begin(), in_node.end(), 
                   inserter(to_remove, to_remove.begin()));

    for(auto it=_sinks.begin(); it < _sinks.end();) {
        if(to_remove.count((*it)->port())) {
            scene()->removeItem(*it);
            delete(*it);
            it = _sinks.erase(it);
        }
        else {
            ++it;
        }
    }
    for(auto it=_sources.begin(); it < _sources.end();) {
        if(to_remove.count((*it)->port())) {
            scene()->removeItem(*it);
            delete(*it);
            it = _sources.erase(it);
        }
        else {
            ++it;
        }
    }

    for (auto port : to_add) {
        add_socket(port);
    }
}

void GraphicsNode::
updateNode(QString name)
{
    _node->name(name.toStdString());
}

void GraphicsNode::
updateGeometry()
{
    if (!_changed) return;

    // compute if we have reached the minimum size
    updateSizeHints();
    _width = std::max(_min_width, _width);
    _height = std::max(_min_height, _height);

    // title
    _title_item->setTextWidth(_width);

    qreal ypos1 = _top_margin;
    for (size_t i = 0; i < _sinks.size(); i++) {
        auto s = _sinks[i];
        auto size = s->getSize();

        // sockets are centered around 0/0
        s->setPos(0, ypos1 + size.height()/2.0);
        ypos1 += size.height() + _item_padding;
    }

    // sources are placed bottom/right
    qreal ypos2 = _height - _bottom_margin;
    for (size_t i = _sources.size(); i > 0; i--) {
        auto s = _sources[i-1];
        auto size = s->getSize();

        ypos2 -= size.height();
        s->setPos(_width, ypos2 + size.height()/2.0);
        ypos2 -= _item_padding;
    }


    // central widget
    if (_central_proxy != nullptr) {
        QRectF geom(_lr_padding, ypos1, _width - 2.0 * _lr_padding, ypos2 - ypos1);
        _central_proxy->setGeometry(geom);
    }

    _changed = false;
    propagateChanges();
}


void GraphicsNode::
setCentralWidget (QWidget *widget)
{
    if (_central_proxy)
        delete _central_proxy;
    _central_proxy = new QGraphicsProxyWidget(this);
    _central_proxy->setWidget(widget);
    _changed = true;
    prepareGeometryChange();
    updateGeometry();
}


void GraphicsNode::
updateSizeHints() {
    qreal min_width = 0.0;// _hard_min_width;
    qreal min_height = _top_margin + _bottom_margin; // _hard_min_height;

    // sinks
    for (size_t i = 0; i < _sinks.size(); i++) {
        auto s = _sinks[i];
        auto size = s->getMinimalSize();

        min_height += size.height() + _item_padding;
        min_width = std::max(size.width(), min_width);
    }

    // central widget
    if (_central_proxy != nullptr) {
        auto wgt = _central_proxy->widget();
        if (wgt) {
            // only take the size hint if the value is valid, and if
            // the minimumSize is not set (similar to
            // QWidget/QLayout standard behavior
            auto sh = wgt->minimumSizeHint();
            auto sz = wgt->minimumSize();
            if (sh.isValid()) {
                if (sz.height() > 0)
                    min_height += sz.height();
                else
                    min_height += sh.height();

                if (sz.width() > 0)
                    min_width = std::max(qreal(sz.width()) + 2.0*_lr_padding, min_width);
                else
                    min_width = std::max(qreal(sh.width()) + 2.0*_lr_padding, min_width);
            } else {
                min_height += sh.height();
                min_width = std::max(qreal(sh.width()) + 2.0*_lr_padding, min_width);
            }
        }
    }

    // sources
    for (size_t i = 0; i < _sources.size(); i++) {
        auto s = _sources[i];
        auto size = s->getMinimalSize();

        min_height += size.height() + _item_padding;
        min_width = std::max(size.width(), min_width);
    }

    _min_width = std::max(min_width, _hard_min_width);
    _min_height = std::max(min_height, _hard_min_height);
}


void GraphicsNode::
propagateChanges()
{
    for (auto sink: _sinks)
        sink->notifyPositionChange();

    for (auto source: _sources)
        source->notifyPositionChange();
}



void GraphicsNode::
mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    qWarning() << "Double clicked!";

    //auto popup = make_shared<QLineEdit>();
    //popup->show();

    QGraphicsItem::mouseDoubleClickEvent(event);
}



