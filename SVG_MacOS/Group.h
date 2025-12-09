//Group.h
#ifndef GROUP_H
#define GROUP_H

#include "SVGElement.h"
#include <vector>
#include <memory>

class SVGRenderer;

class Group : public SVGElement
{
private:
    std::vector<std::shared_ptr<SVGElement>> children_;

public:
    Group(const Attributes& attributes);

    void addElement(std::shared_ptr<SVGElement> element);

    void draw(SVGRenderer& renderer) const override;

    std::string getElementName() const override { return "g"; }
};

#endif
