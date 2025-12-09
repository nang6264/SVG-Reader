// Group.cpp
#include "Group.h"
#include "SVGRenderer.h"

Group::Group(const Attributes& attributes) : SVGElement(attributes)
{
}

void Group::addElement(std::shared_ptr<SVGElement> element) {
    if (element)
    {
        children_.push_back(element);
    }
}

void Group::draw(SVGRenderer& renderer) const
{
    // Lớp Group là SVGElement nên nó có thuộc tính cục bộ (this->transform_, this->attributes_)
    renderer.beginElement(this->transform_, this->attributes_);

    // Vẽ các phần tử con đệ quy (chúng sẽ tự động dùng Transform từ Stack)
    for (const auto& child : children_)
    {
        child->draw(renderer);
    }

    // Xóa Context của Group khỏi Stack
    renderer.endElement();
}
