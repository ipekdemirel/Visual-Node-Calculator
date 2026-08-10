#include "NodeEditor.h"

#include "imgui.h"
#include "imnodes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    enum class NodeType
    {
        Number,
        Add,
        Subtract,
        Multiply,
        Divide,
        Result,
        Power,
        Modulo,
        SquareRoot,
        Absolute,
        Sine,
        Cosine
    };

    struct Node
    {
        int id = 0;
        NodeType type = NodeType::Number;
        float value = 0.0f;
    };

    struct Link
    {
        int id = 0;
        int startAttribute = 0;
        int endAttribute = 0;
    };

    struct EvaluationResult
    {
        bool success = false;
        float value = 0.0f;
        std::string message = "Not connected";
    };

    std::vector<Node> g_nodes;
    std::vector<Link> g_links;

    int g_nextNodeId = 1;
    int g_nextLinkId = 1000;

    bool g_showMiniMap = true;
    bool g_placeNewNode = false;
    int g_newNodeId = -1;
    ImVec2 g_newNodePosition(0.0f, 0.0f);

    std::string g_statusMessage =
        "Hazir: Bos alana sag tiklayarak node olusturabilirsin.";

    char g_expressionBuffer[256] = "((10 + 5) * 2 - 3) / 2";
    int g_lastResultNodeId = -1;

    constexpr const char* kGraphFileName =
        "VisualNodeCalculatorGraph.vnc";

    bool g_unsavedChanges = false;

    float g_zoom = 1.0f;

    constexpr float kMinimumZoom = 0.55f;
    constexpr float kMaximumZoom = 1.60f;
    constexpr float kZoomStep = 0.10f;

    struct ClipboardNode
    {
        NodeType type = NodeType::Number;
        float value = 0.0f;
        ImVec2 relativePosition = ImVec2(0.0f, 0.0f);
        int originalNodeId = -1;
    };

    struct ClipboardLink
    {
        int originalStartNodeId = -1;
        int originalEndNodeId = -1;
        int startPinOffset = 0;
        int endPinOffset = 0;
    };

    std::vector<ClipboardNode> g_clipboardNodes;
    std::vector<ClipboardLink> g_clipboardLinks;

    int g_pasteCount = 0;

    constexpr int kInputAOffset = 1;
    constexpr int kInputBOffset = 2;
    constexpr int kOutputOffset = 3;
    constexpr int kStaticOffset = 4;

    int InputAId(const Node& node)
    {
        return node.id * 100 + kInputAOffset;
    }

    int InputBId(const Node& node)
    {
        return node.id * 100 + kInputBOffset;
    }

    int OutputId(const Node& node)
    {
        return node.id * 100 + kOutputOffset;
    }

    int StaticAttributeId(const Node& node)
    {
        return node.id * 100 + kStaticOffset;
    }

    int NodeIdFromAttribute(int attributeId)
    {
        return attributeId / 100;
    }

    Node* FindNode(int nodeId)
    {
        for (Node& node : g_nodes)
        {
            if (node.id == nodeId)
            {
                return &node;
            }
        }

        return nullptr;
    }

    const Node* FindNodeConst(int nodeId)
    {
        for (const Node& node : g_nodes)
        {
            if (node.id == nodeId)
            {
                return &node;
            }
        }

        return nullptr;
    }

    const Node* FindNodeFromAttribute(int attributeId)
    {
        return FindNodeConst(NodeIdFromAttribute(attributeId));
    }

    bool HasOutput(NodeType type)
    {
        return type != NodeType::Result;
    }

    bool HasInputA(NodeType type)
    {
        return type != NodeType::Number;
    }

    bool HasInputB(NodeType type)
    {
        return
            type == NodeType::Add ||
            type == NodeType::Subtract ||
            type == NodeType::Multiply ||
            type == NodeType::Divide ||
            type == NodeType::Power ||
            type == NodeType::Modulo;
    }

    bool IsOutputAttribute(int attributeId)
    {
        const Node* node = FindNodeFromAttribute(attributeId);

        return
            node != nullptr &&
            HasOutput(node->type) &&
            attributeId == OutputId(*node);
    }

    bool IsInputAttribute(int attributeId)
    {
        const Node* node = FindNodeFromAttribute(attributeId);

        if (node == nullptr)
        {
            return false;
        }

        if (
            HasInputA(node->type) &&
            attributeId == InputAId(*node)
            )
        {
            return true;
        }

        return
            HasInputB(node->type) &&
            attributeId == InputBId(*node);
    }

    const Link* FindLinkToInput(int inputAttributeId)
    {
        for (const Link& link : g_links)
        {
            if (link.endAttribute == inputAttributeId)
            {
                return &link;
            }
        }

        return nullptr;
    }

    bool InputAlreadyConnected(int inputAttributeId)
    {
        return FindLinkToInput(inputAttributeId) != nullptr;
    }

    bool LinkAlreadyExists(int startAttribute, int endAttribute)
    {
        for (const Link& link : g_links)
        {
            if (
                link.startAttribute == startAttribute &&
                link.endAttribute == endAttribute
                )
            {
                return true;
            }
        }

        return false;
    }

    bool IsConnectionAllowed(int startAttribute, int endAttribute)
    {
        if (
            !IsOutputAttribute(startAttribute) ||
            !IsInputAttribute(endAttribute)
            )
        {
            g_statusMessage =
                "Baglanti reddedildi: Bir output pinini bir input pinine baglamalisin.";
            return false;
        }

        if (
            NodeIdFromAttribute(startAttribute) ==
            NodeIdFromAttribute(endAttribute)
            )
        {
            g_statusMessage =
                "Baglanti reddedildi: Bir node kendisine baglanamaz.";
            return false;
        }

        if (InputAlreadyConnected(endAttribute))
        {
            g_statusMessage =
                "Baglanti reddedildi: Bu input pini zaten bagli.";
            return false;
        }

        if (LinkAlreadyExists(startAttribute, endAttribute))
        {
            g_statusMessage =
                "Baglanti reddedildi: Bu baglanti zaten var.";
            return false;
        }

        return true;
    }

    void AddLink(int startAttribute, int endAttribute)
    {
        g_links.push_back(
            {
                g_nextLinkId++,
                startAttribute,
                endAttribute
            }
        );

        g_statusMessage = "Baglanti olusturuldu.";
        g_unsavedChanges = true;
    }

    int AddNode(
        NodeType type,
        const ImVec2& screenPosition,
        float initialValue = 0.0f
    )
    {
        Node node;
        node.id = g_nextNodeId++;
        node.type = type;
        node.value = initialValue;

        g_nodes.push_back(node);

        g_newNodeId = node.id;
        g_newNodePosition = screenPosition;
        g_placeNewNode = true;

        g_statusMessage = "Yeni node olusturuldu.";
        g_unsavedChanges = true;

        return node.id;
    }

    EvaluationResult EvaluateOutput(
        int outputAttribute,
        std::unordered_set<int>& visitingNodes
    );

    EvaluationResult EvaluateInput(
        int inputAttribute,
        std::unordered_set<int>& visitingNodes
    )
    {
        const Link* link = FindLinkToInput(inputAttribute);

        if (link == nullptr)
        {
            return { false, 0.0f, "Input bagli degil" };
        }

        return EvaluateOutput(
            link->startAttribute,
            visitingNodes
        );
    }

    EvaluationResult EvaluateOutput(
        int outputAttribute,
        std::unordered_set<int>& visitingNodes
    )
    {
        const Node* node = FindNodeFromAttribute(outputAttribute);

        if (
            node == nullptr ||
            outputAttribute != OutputId(*node)
            )
        {
            return { false, 0.0f, "Gecersiz output" };
        }

        if (visitingNodes.find(node->id) != visitingNodes.end())
        {
            return { false, 0.0f, "Dongusel baglanti" };
        }

        if (node->type == NodeType::Number)
        {
            return { true, node->value, "Hazir" };
        }

        visitingNodes.insert(node->id);

        const EvaluationResult inputA =
            EvaluateInput(InputAId(*node), visitingNodes);

        if (!inputA.success)
        {
            visitingNodes.erase(node->id);
            return inputA;
        }

        EvaluationResult result;

        // Tek girdili gelismis matematik node'lari
        switch (node->type)
        {
        case NodeType::SquareRoot:
            if (inputA.value < 0.0f)
            {
                result =
                {
                    false,
                    0.0f,
                    "Negatif sayinin karekoku alinamaz"
                };
            }
            else
            {
                result =
                {
                    true,
                    std::sqrt(inputA.value),
                    "Hazir"
                };
            }

            visitingNodes.erase(node->id);
            return result;

        case NodeType::Absolute:
            result =
            {
                true,
                std::fabs(inputA.value),
                "Hazir"
            };

            visitingNodes.erase(node->id);
            return result;

        case NodeType::Sine:
            result =
            {
                true,
                std::sin(inputA.value),
                "Hazir"
            };

            visitingNodes.erase(node->id);
            return result;

        case NodeType::Cosine:
            result =
            {
                true,
                std::cos(inputA.value),
                "Hazir"
            };

            visitingNodes.erase(node->id);
            return result;

        default:
            break;
        }

        const EvaluationResult inputB =
            EvaluateInput(InputBId(*node), visitingNodes);

        if (!inputB.success)
        {
            visitingNodes.erase(node->id);
            return inputB;
        }

        switch (node->type)
        {
        case NodeType::Add:
            result = { true, inputA.value + inputB.value, "Hazir" };
            break;

        case NodeType::Subtract:
            result = { true, inputA.value - inputB.value, "Hazir" };
            break;

        case NodeType::Multiply:
            result = { true, inputA.value * inputB.value, "Hazir" };
            break;

        case NodeType::Divide:
            if (std::fabs(inputB.value) < 0.000001f)
            {
                result = { false, 0.0f, "Sifira bolme hatasi" };
            }
            else
            {
                result = { true, inputA.value / inputB.value, "Hazir" };
            }
            break;

        case NodeType::Power:
            result =
            {
                true,
                std::pow(inputA.value, inputB.value),
                "Hazir"
            };
            break;

        case NodeType::Modulo:
            if (std::fabs(inputB.value) < 0.000001f)
            {
                result =
                {
                    false,
                    0.0f,
                    "Modulo icin ikinci deger sifir olamaz"
                };
            }
            else
            {
                result =
                {
                    true,
                    std::fmod(inputA.value, inputB.value),
                    "Hazir"
                };
            }
            break;

        default:
            result = { false, 0.0f, "Desteklenmeyen islem" };
            break;
        }

        visitingNodes.erase(node->id);
        return result;
    }

    EvaluationResult EvaluateResultNode(const Node& node)
    {
        std::unordered_set<int> visitingNodes;

        return EvaluateInput(
            InputAId(node),
            visitingNodes
        );
    }

    void DeleteLinkById(int linkId)
    {
        g_links.erase(
            std::remove_if(
                g_links.begin(),
                g_links.end(),
                [linkId](const Link& link)
                {
                    return link.id == linkId;
                }
            ),
            g_links.end()
        );

        g_statusMessage = "Baglanti silindi.";
        g_unsavedChanges = true;
    }

    void DeleteNodeById(int nodeId)
    {
        g_links.erase(
            std::remove_if(
                g_links.begin(),
                g_links.end(),
                [nodeId](const Link& link)
                {
                    return
                        NodeIdFromAttribute(link.startAttribute) == nodeId ||
                        NodeIdFromAttribute(link.endAttribute) == nodeId;
                }
            ),
            g_links.end()
        );

        g_nodes.erase(
            std::remove_if(
                g_nodes.begin(),
                g_nodes.end(),
                [nodeId](const Node& node)
                {
                    return node.id == nodeId;
                }
            ),
            g_nodes.end()
        );

        g_statusMessage = "Node silindi.";
        g_unsavedChanges = true;
    }

    void DeleteSelectedElements()
    {
        const int selectedLinkCount = ImNodes::NumSelectedLinks();

        if (selectedLinkCount > 0)
        {
            std::vector<int> selectedLinks(
                static_cast<size_t>(selectedLinkCount)
            );

            ImNodes::GetSelectedLinks(selectedLinks.data());

            for (const int linkId : selectedLinks)
            {
                DeleteLinkById(linkId);
            }
        }

        const int selectedNodeCount = ImNodes::NumSelectedNodes();

        if (selectedNodeCount > 0)
        {
            std::vector<int> selectedNodes(
                static_cast<size_t>(selectedNodeCount)
            );

            ImNodes::GetSelectedNodes(selectedNodes.data());

            for (const int nodeId : selectedNodes)
            {
                DeleteNodeById(nodeId);
            }
        }

        ImNodes::ClearLinkSelection();
        ImNodes::ClearNodeSelection();
    }

    void ClearGraph()
    {
        g_nodes.clear();
        g_links.clear();

        g_nextNodeId = 1;
        g_nextLinkId = 1000;
        g_lastResultNodeId = -1;

        g_statusMessage = "Calisma alani temizlendi.";
        g_unsavedChanges = true;
    }

    void LoadExampleGraph()
    {
        ClearGraph();

        Node numberA{ g_nextNodeId++, NodeType::Number, 12.0f };
        Node numberB{ g_nextNodeId++, NodeType::Number, 4.0f };
        Node add{ g_nextNodeId++, NodeType::Add, 0.0f };
        Node result{ g_nextNodeId++, NodeType::Result, 0.0f };

        g_nodes.push_back(numberA);
        g_nodes.push_back(numberB);
        g_nodes.push_back(add);
        g_nodes.push_back(result);
        g_lastResultNodeId = result.id;

        AddLink(OutputId(numberA), InputAId(add));
        AddLink(OutputId(numberB), InputBId(add));
        AddLink(OutputId(add), InputAId(result));

        ImNodes::SetNodeGridSpacePos(
            numberA.id,
            ImVec2(110.0f, 120.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            numberB.id,
            ImVec2(110.0f, 410.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            add.id,
            ImVec2(480.0f, 260.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            result.id,
            ImVec2(850.0f, 280.0f)
        );

        g_statusMessage = "Ornek graph yuklendi.";
        g_unsavedChanges = true;
    }

    void LoadComplexGraph()
    {
        ClearGraph();

        Node numberA{ g_nextNodeId++, NodeType::Number, 10.0f };
        Node numberB{ g_nextNodeId++, NodeType::Number, 5.0f };
        Node numberC{ g_nextNodeId++, NodeType::Number, 2.0f };
        Node numberD{ g_nextNodeId++, NodeType::Number, 3.0f };

        Node add{ g_nextNodeId++, NodeType::Add, 0.0f };
        Node multiply{ g_nextNodeId++, NodeType::Multiply, 0.0f };
        Node subtract{ g_nextNodeId++, NodeType::Subtract, 0.0f };
        Node divide{ g_nextNodeId++, NodeType::Divide, 0.0f };
        Node result{ g_nextNodeId++, NodeType::Result, 0.0f };

        g_nodes.push_back(numberA);
        g_nodes.push_back(numberB);
        g_nodes.push_back(numberC);
        g_nodes.push_back(numberD);
        g_nodes.push_back(add);
        g_nodes.push_back(multiply);
        g_nodes.push_back(subtract);
        g_nodes.push_back(divide);
        g_nodes.push_back(result);
        g_lastResultNodeId = result.id;

        // 10 + 5 = 15
        AddLink(OutputId(numberA), InputAId(add));
        AddLink(OutputId(numberB), InputBId(add));

        // 15 * 2 = 30
        AddLink(OutputId(add), InputAId(multiply));
        AddLink(OutputId(numberC), InputBId(multiply));

        // 30 - 3 = 27
        AddLink(OutputId(multiply), InputAId(subtract));
        AddLink(OutputId(numberD), InputBId(subtract));

        // 27 / 2 = 13.5
        // Number C burada ikinci kez kullanilarak graph dallandiriliyor.
        AddLink(OutputId(subtract), InputAId(divide));
        AddLink(OutputId(numberC), InputBId(divide));

        AddLink(OutputId(divide), InputAId(result));

        ImNodes::SetNodeGridSpacePos(
            numberA.id,
            ImVec2(60.0f, 80.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            numberB.id,
            ImVec2(60.0f, 280.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            numberC.id,
            ImVec2(60.0f, 500.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            numberD.id,
            ImVec2(460.0f, 520.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            add.id,
            ImVec2(390.0f, 150.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            multiply.id,
            ImVec2(720.0f, 220.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            subtract.id,
            ImVec2(1030.0f, 310.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            divide.id,
            ImVec2(1320.0f, 390.0f)
        );

        ImNodes::SetNodeGridSpacePos(
            result.id,
            ImVec2(1610.0f, 410.0f)
        );

        g_statusMessage =
            "Karmasik ornek yuklendi: ((10 + 5) * 2 - 3) / 2 = 13.5";
        g_unsavedChanges = true;
    }

    struct ParsedExpressionNode
    {
        bool isNumber = false;
        float numberValue = 0.0f;
        char operation = 0;
        int left = -1;
        int right = -1;
    };

    class ExpressionParser
    {
    public:
        explicit ExpressionParser(const char* text)
            : m_text(text != nullptr ? text : "")
        {
        }

        bool Parse(int& rootIndex, std::string& error)
        {
            m_position = 0;
            m_nodes.clear();

            rootIndex = ParseExpression(error);

            if (rootIndex < 0)
            {
                return false;
            }

            SkipSpaces();

            if (m_position != m_text.size())
            {
                error = "Gecersiz karakter: '";
                error += m_text[m_position];
                error += "'";
                return false;
            }

            return true;
        }

        const std::vector<ParsedExpressionNode>& Nodes() const
        {
            return m_nodes;
        }

    private:
        int ParseExpression(std::string& error)
        {
            int left = ParseTerm(error);

            if (left < 0)
            {
                return -1;
            }

            while (true)
            {
                SkipSpaces();

                if (!Match('+') && !Match('-'))
                {
                    break;
                }

                const char operation = m_text[m_position - 1];
                const int right = ParseTerm(error);

                if (right < 0)
                {
                    return -1;
                }

                left = AddOperationNode(operation, left, right);
            }

            return left;
        }

        int ParseTerm(std::string& error)
        {
            int left = ParseFactor(error);

            if (left < 0)
            {
                return -1;
            }

            while (true)
            {
                SkipSpaces();

                if (!Match('*') && !Match('/'))
                {
                    break;
                }

                const char operation = m_text[m_position - 1];
                const int right = ParseFactor(error);

                if (right < 0)
                {
                    return -1;
                }

                left = AddOperationNode(operation, left, right);
            }

            return left;
        }

        int ParseFactor(std::string& error)
        {
            SkipSpaces();

            if (Match('('))
            {
                const int expression = ParseExpression(error);

                if (expression < 0)
                {
                    return -1;
                }

                SkipSpaces();

                if (!Match(')'))
                {
                    error = "Kapanmayan parantez var.";
                    return -1;
                }

                return expression;
            }

            if (Match('-'))
            {
                const int value = ParseFactor(error);

                if (value < 0)
                {
                    return -1;
                }

                const int zero = AddNumberNode(0.0f);
                return AddOperationNode('-', zero, value);
            }

            return ParseNumber(error);
        }

        int ParseNumber(std::string& error)
        {
            SkipSpaces();

            const char* begin = m_text.c_str() + m_position;
            char* end = nullptr;
            const float value = std::strtof(begin, &end);

            if (end == begin)
            {
                error = "Bir sayi bekleniyordu.";
                return -1;
            }

            m_position += static_cast<size_t>(end - begin);
            return AddNumberNode(value);
        }

        int AddNumberNode(float value)
        {
            ParsedExpressionNode node;
            node.isNumber = true;
            node.numberValue = value;

            m_nodes.push_back(node);
            return static_cast<int>(m_nodes.size()) - 1;
        }

        int AddOperationNode(char operation, int left, int right)
        {
            ParsedExpressionNode node;
            node.operation = operation;
            node.left = left;
            node.right = right;

            m_nodes.push_back(node);
            return static_cast<int>(m_nodes.size()) - 1;
        }

        void SkipSpaces()
        {
            while (
                m_position < m_text.size() &&
                std::isspace(
                    static_cast<unsigned char>(m_text[m_position])
                )
                )
            {
                ++m_position;
            }
        }

        bool Match(char character)
        {
            if (
                m_position < m_text.size() &&
                m_text[m_position] == character
                )
            {
                ++m_position;
                return true;
            }

            return false;
        }

        std::string m_text;
        size_t m_position = 0;
        std::vector<ParsedExpressionNode> m_nodes;
    };

    NodeType OperationType(char operation)
    {
        switch (operation)
        {
        case '+': return NodeType::Add;
        case '-': return NodeType::Subtract;
        case '*': return NodeType::Multiply;
        case '/': return NodeType::Divide;
        default:  return NodeType::Add;
        }
    }

    struct BuiltExpressionNode
    {
        int graphNodeId = -1;
        float y = 0.0f;
    };

    int ExpressionDepth(
        const std::vector<ParsedExpressionNode>& parsedNodes,
        int parsedIndex
    )
    {
        const ParsedExpressionNode& node = parsedNodes[parsedIndex];

        if (node.isNumber)
        {
            return 0;
        }

        return 1 + std::max(
            ExpressionDepth(parsedNodes, node.left),
            ExpressionDepth(parsedNodes, node.right)
        );
    }

    BuiltExpressionNode BuildExpressionNode(
        const std::vector<ParsedExpressionNode>& parsedNodes,
        int parsedIndex,
        int maximumDepth,
        int depth,
        float& nextLeafY
    )
    {
        const ParsedExpressionNode& parsed = parsedNodes[parsedIndex];

        if (parsed.isNumber)
        {
            Node number;
            number.id = g_nextNodeId++;
            number.type = NodeType::Number;
            number.value = parsed.numberValue;

            g_nodes.push_back(number);

            const float y = nextLeafY;
            nextLeafY += 190.0f;

            ImNodes::SetNodeGridSpacePos(
                number.id,
                ImVec2(70.0f, y)
            );

            return { number.id, y };
        }

        const BuiltExpressionNode left = BuildExpressionNode(
            parsedNodes,
            parsed.left,
            maximumDepth,
            depth + 1,
            nextLeafY
        );

        const BuiltExpressionNode right = BuildExpressionNode(
            parsedNodes,
            parsed.right,
            maximumDepth,
            depth + 1,
            nextLeafY
        );

        Node operation;
        operation.id = g_nextNodeId++;
        operation.type = OperationType(parsed.operation);

        g_nodes.push_back(operation);

        const float y = (left.y + right.y) * 0.5f;
        const float x =
            70.0f +
            static_cast<float>(maximumDepth - depth + 1) * 310.0f;

        ImNodes::SetNodeGridSpacePos(
            operation.id,
            ImVec2(x, y)
        );

        const Node* leftNode = FindNodeConst(left.graphNodeId);
        const Node* rightNode = FindNodeConst(right.graphNodeId);

        if (leftNode != nullptr && rightNode != nullptr)
        {
            AddLink(OutputId(*leftNode), InputAId(operation));
            AddLink(OutputId(*rightNode), InputBId(operation));
        }

        return { operation.id, y };
    }

    bool BuildGraphFromExpression(const char* expression)
    {
        ExpressionParser parser(expression);
        std::string error;
        int rootIndex = -1;

        if (!parser.Parse(rootIndex, error))
        {
            g_statusMessage = "Komut hatasi: " + error;
            return false;
        }

        ClearGraph();

        const std::vector<ParsedExpressionNode>& parsedNodes =
            parser.Nodes();

        const int maximumDepth =
            ExpressionDepth(parsedNodes, rootIndex);

        float nextLeafY = 90.0f;

        const BuiltExpressionNode root = BuildExpressionNode(
            parsedNodes,
            rootIndex,
            maximumDepth,
            0,
            nextLeafY
        );

        Node result;
        result.id = g_nextNodeId++;
        result.type = NodeType::Result;

        g_nodes.push_back(result);
        g_lastResultNodeId = result.id;

        const Node* rootNode = FindNodeConst(root.graphNodeId);

        if (rootNode != nullptr)
        {
            AddLink(OutputId(*rootNode), InputAId(result));
        }

        ImNodes::SetNodeGridSpacePos(
            result.id,
            ImVec2(
                70.0f +
                static_cast<float>(maximumDepth + 2) * 310.0f,
                root.y
            )
        );

        g_statusMessage =
            "Komut graph'a donusturuldu: " + std::string(expression);
        g_unsavedChanges = true;

        ImNodes::EditorContextMoveToNode(result.id);
        return true;
    }

    void ApplyZoomStyle()
    {
        ImNodesStyle& style = ImNodes::GetStyle();

        style.GridSpacing = 32.0f * g_zoom;
        style.NodeCornerRounding = 12.0f * g_zoom;
        style.NodePadding =
            ImVec2(
                14.0f * g_zoom,
                12.0f * g_zoom
            );

        style.NodeBorderThickness =
            std::max(1.0f, 1.5f * g_zoom);

        style.LinkThickness =
            std::max(2.0f, 4.0f * g_zoom);

        style.LinkHoverDistance =
            std::max(10.0f, 16.0f * g_zoom);

        style.PinCircleRadius =
            std::max(5.0f, 8.0f * g_zoom);

        style.PinHoverRadius =
            std::max(12.0f, 22.0f * g_zoom);
    }

    void SetGraphZoom(float requestedZoom)
    {
        const float newZoom =
            std::clamp(
                requestedZoom,
                kMinimumZoom,
                kMaximumZoom
            );

        if (std::fabs(newZoom - g_zoom) < 0.0001f)
        {
            return;
        }

        const float ratio = newZoom / g_zoom;

        if (!g_nodes.empty())
        {
            ImVec2 center(0.0f, 0.0f);

            for (const Node& node : g_nodes)
            {
                const ImVec2 position =
                    ImNodes::GetNodeGridSpacePos(node.id);

                center.x += position.x;
                center.y += position.y;
            }

            center.x /= static_cast<float>(g_nodes.size());
            center.y /= static_cast<float>(g_nodes.size());

            for (const Node& node : g_nodes)
            {
                const ImVec2 position =
                    ImNodes::GetNodeGridSpacePos(node.id);

                const ImVec2 scaledPosition(
                    center.x +
                    (position.x - center.x) * ratio,
                    center.y +
                    (position.y - center.y) * ratio
                );

                ImNodes::SetNodeGridSpacePos(
                    node.id,
                    scaledPosition
                );
            }
        }

        g_zoom = newZoom;
        ApplyZoomStyle();
        g_unsavedChanges = true;

        const int percentage =
            static_cast<int>(g_zoom * 100.0f + 0.5f);

        g_statusMessage =
            "Zoom: %" + std::to_string(percentage);
    }

    void ResetGraphZoom()
    {
        SetGraphZoom(1.0f);
    }

    int AttributeOffset(int attributeId)
    {
        return attributeId % 100;
    }

    void CopySelectedNodes()
    {
        const int selectedNodeCount =
            ImNodes::NumSelectedNodes();

        if (selectedNodeCount <= 0)
        {
            g_statusMessage =
                "Kopyalama: Once en az bir node secmelisin.";
            return;
        }

        std::vector<int> selectedNodeIds(
            static_cast<size_t>(selectedNodeCount)
        );

        ImNodes::GetSelectedNodes(
            selectedNodeIds.data()
        );

        g_clipboardNodes.clear();
        g_clipboardLinks.clear();

        float minimumX = 0.0f;
        float minimumY = 0.0f;
        bool firstPosition = true;

        std::unordered_set<int> selectedSet;

        for (const int nodeId : selectedNodeIds)
        {
            selectedSet.insert(nodeId);

            const Node* node = FindNodeConst(nodeId);

            if (node == nullptr)
            {
                continue;
            }

            const ImVec2 position =
                ImNodes::GetNodeGridSpacePos(nodeId);

            if (firstPosition)
            {
                minimumX = position.x;
                minimumY = position.y;
                firstPosition = false;
            }
            else
            {
                minimumX = std::min(minimumX, position.x);
                minimumY = std::min(minimumY, position.y);
            }
        }

        for (const int nodeId : selectedNodeIds)
        {
            const Node* node = FindNodeConst(nodeId);

            if (node == nullptr)
            {
                continue;
            }

            const ImVec2 position =
                ImNodes::GetNodeGridSpacePos(nodeId);

            ClipboardNode copiedNode;
            copiedNode.type = node->type;
            copiedNode.value = node->value;
            copiedNode.relativePosition =
                ImVec2(
                    position.x - minimumX,
                    position.y - minimumY
                );
            copiedNode.originalNodeId = node->id;

            g_clipboardNodes.push_back(copiedNode);
        }

        for (const Link& link : g_links)
        {
            const int startNodeId =
                NodeIdFromAttribute(link.startAttribute);

            const int endNodeId =
                NodeIdFromAttribute(link.endAttribute);

            if (
                selectedSet.find(startNodeId) ==
                selectedSet.end() ||
                selectedSet.find(endNodeId) ==
                selectedSet.end()
                )
            {
                continue;
            }

            ClipboardLink copiedLink;
            copiedLink.originalStartNodeId = startNodeId;
            copiedLink.originalEndNodeId = endNodeId;
            copiedLink.startPinOffset =
                AttributeOffset(link.startAttribute);
            copiedLink.endPinOffset =
                AttributeOffset(link.endAttribute);

            g_clipboardLinks.push_back(copiedLink);
        }

        g_pasteCount = 0;

        g_statusMessage =
            std::to_string(g_clipboardNodes.size()) +
            " node kopyalandi.";
    }

    void PasteClipboardNodes()
    {
        if (g_clipboardNodes.empty())
        {
            g_statusMessage =
                "Yapistirma: Kopyalanmis node yok.";
            return;
        }

        ++g_pasteCount;

        const ImVec2 mousePosition =
            ImGui::GetMousePos();

        const float extraOffset =
            static_cast<float>(g_pasteCount - 1) * 28.0f;

        std::unordered_map<int, int> idMap;
        std::vector<int> pastedNodeIds;

        pastedNodeIds.reserve(g_clipboardNodes.size());

        for (const ClipboardNode& copiedNode :
            g_clipboardNodes)
        {
            Node node;
            node.id = g_nextNodeId++;
            node.type = copiedNode.type;
            node.value = copiedNode.value;

            g_nodes.push_back(node);

            idMap[copiedNode.originalNodeId] = node.id;
            pastedNodeIds.push_back(node.id);

            ImNodes::SetNodeScreenSpacePos(
                node.id,
                ImVec2(
                    mousePosition.x +
                    copiedNode.relativePosition.x +
                    extraOffset,
                    mousePosition.y +
                    copiedNode.relativePosition.y +
                    extraOffset
                )
            );

            if (node.type == NodeType::Result)
            {
                g_lastResultNodeId = node.id;
            }
        }

        for (const ClipboardLink& copiedLink :
            g_clipboardLinks)
        {
            const auto startIterator =
                idMap.find(copiedLink.originalStartNodeId);

            const auto endIterator =
                idMap.find(copiedLink.originalEndNodeId);

            if (
                startIterator == idMap.end() ||
                endIterator == idMap.end()
                )
            {
                continue;
            }

            const int startAttribute =
                startIterator->second * 100 +
                copiedLink.startPinOffset;

            const int endAttribute =
                endIterator->second * 100 +
                copiedLink.endPinOffset;

            AddLink(startAttribute, endAttribute);
        }

        ImNodes::ClearNodeSelection();
        ImNodes::ClearLinkSelection();

        for (const int nodeId : pastedNodeIds)
        {
            ImNodes::SelectNode(nodeId);
        }

        g_unsavedChanges = true;

        g_statusMessage =
            std::to_string(pastedNodeIds.size()) +
            " node yapistirildi.";
    }

    void DuplicateSelectedNodes()
    {
        CopySelectedNodes();

        if (!g_clipboardNodes.empty())
        {
            PasteClipboardNodes();
        }
    }

    struct SavedNodePosition
    {
        int nodeId = 0;
        ImVec2 position = ImVec2(0.0f, 0.0f);
    };

    bool SaveGraphToFile()
    {
        std::ofstream file(
            kGraphFileName,
            std::ios::out | std::ios::trunc
        );

        if (!file.is_open())
        {
            g_statusMessage =
                "Kaydetme hatasi: Dosya olusturulamadi.";
            return false;
        }

        file << "VNC_GRAPH 2\n";
        file << "MINIMAP " << (g_showMiniMap ? 1 : 0) << '\n';
        file << "EXPRESSION "
            << std::quoted(std::string(g_expressionBuffer))
            << '\n';

        file << "NODES " << g_nodes.size() << '\n';

        file << std::setprecision(9);

        for (const Node& node : g_nodes)
        {
            const ImVec2 position =
                ImNodes::GetNodeGridSpacePos(node.id);

            file
                << node.id << ' '
                << static_cast<int>(node.type) << ' '
                << node.value << ' '
                << position.x << ' '
                << position.y << '\n';
        }

        file << "LINKS " << g_links.size() << '\n';

        for (const Link& link : g_links)
        {
            file
                << link.id << ' '
                << link.startAttribute << ' '
                << link.endAttribute << '\n';
        }

        file << "END\n";

        if (!file.good())
        {
            g_statusMessage =
                "Kaydetme hatasi: Dosyaya yazilamadi.";
            return false;
        }

        g_unsavedChanges = false;

        g_statusMessage =
            std::string("Graph kaydedildi: ") +
            kGraphFileName;

        return true;
    }

    bool LoadGraphFromFile()
    {
        std::ifstream file(kGraphFileName);

        if (!file.is_open())
        {
            g_statusMessage =
                std::string("Yukleme hatasi: ") +
                kGraphFileName +
                " bulunamadi.";
            return false;
        }

        std::string section;
        int version = 0;

        if (
            !(file >> section >> version) ||
            section != "VNC_GRAPH" ||
            (version != 1 && version != 2)
            )
        {
            g_statusMessage =
                "Yukleme hatasi: Gecersiz dosya formati.";
            return false;
        }

        int miniMapValue = 1;

        if (
            !(file >> section >> miniMapValue) ||
            section != "MINIMAP"
            )
        {
            g_statusMessage =
                "Yukleme hatasi: MiniMap bilgisi okunamadi.";
            return false;
        }

        std::string expression;

        if (
            !(file >> section >> std::quoted(expression)) ||
            section != "EXPRESSION"
            )
        {
            g_statusMessage =
                "Yukleme hatasi: Expression okunamadi.";
            return false;
        }

        size_t nodeCount = 0;

        if (
            !(file >> section >> nodeCount) ||
            section != "NODES"
            )
        {
            g_statusMessage =
                "Yukleme hatasi: Node listesi okunamadi.";
            return false;
        }

        std::vector<Node> loadedNodes;
        std::vector<SavedNodePosition> loadedPositions;

        loadedNodes.reserve(nodeCount);
        loadedPositions.reserve(nodeCount);

        int maximumNodeId = 0;
        int loadedResultNodeId = -1;

        for (size_t index = 0; index < nodeCount; ++index)
        {
            int nodeId = 0;
            int nodeTypeValue = 0;
            float value = 0.0f;
            float x = 0.0f;
            float y = 0.0f;

            if (
                !(file
                    >> nodeId
                    >> nodeTypeValue
                    >> value
                    >> x
                    >> y)
                )
            {
                g_statusMessage =
                    "Yukleme hatasi: Node verisi eksik.";
                return false;
            }

            if (
                nodeTypeValue <
                static_cast<int>(NodeType::Number) ||
                nodeTypeValue >
                static_cast<int>(NodeType::Cosine)
                )
            {
                g_statusMessage =
                    "Yukleme hatasi: Bilinmeyen node turu.";
                return false;
            }

            Node node;
            node.id = nodeId;
            node.type = static_cast<NodeType>(nodeTypeValue);
            node.value = value;

            loadedNodes.push_back(node);

            loadedPositions.push_back(
                {
                    nodeId,
                    ImVec2(x, y)
                }
            );

            maximumNodeId =
                std::max(maximumNodeId, nodeId);

            if (node.type == NodeType::Result)
            {
                loadedResultNodeId = node.id;
            }
        }

        size_t linkCount = 0;

        if (
            !(file >> section >> linkCount) ||
            section != "LINKS"
            )
        {
            g_statusMessage =
                "Yukleme hatasi: Link listesi okunamadi.";
            return false;
        }

        std::vector<Link> loadedLinks;
        loadedLinks.reserve(linkCount);

        int maximumLinkId = 999;

        for (size_t index = 0; index < linkCount; ++index)
        {
            Link link;

            if (
                !(file
                    >> link.id
                    >> link.startAttribute
                    >> link.endAttribute)
                )
            {
                g_statusMessage =
                    "Yukleme hatasi: Link verisi eksik.";
                return false;
            }

            loadedLinks.push_back(link);

            maximumLinkId =
                std::max(maximumLinkId, link.id);
        }

        if (!(file >> section) || section != "END")
        {
            g_statusMessage =
                "Yukleme hatasi: Dosya sonu gecersiz.";
            return false;
        }

        g_nodes = std::move(loadedNodes);
        g_links = std::move(loadedLinks);

        g_nextNodeId = maximumNodeId + 1;
        g_nextLinkId = maximumLinkId + 1;
        g_lastResultNodeId = loadedResultNodeId;
        g_showMiniMap = miniMapValue != 0;

        std::snprintf(
            g_expressionBuffer,
            sizeof(g_expressionBuffer),
            "%s",
            expression.c_str()
        );

        for (const SavedNodePosition& saved :
            loadedPositions)
        {
            ImNodes::SetNodeGridSpacePos(
                saved.nodeId,
                saved.position
            );
        }

        ImNodes::ClearNodeSelection();
        ImNodes::ClearLinkSelection();

        g_unsavedChanges = false;

        g_statusMessage =
            std::string("Graph yuklendi: ") +
            kGraphFileName;

        if (g_lastResultNodeId >= 0)
        {
            ImNodes::EditorContextMoveToNode(
                g_lastResultNodeId
            );
        }

        return true;
    }

    const char* NodeTitle(NodeType type)
    {
        switch (type)
        {
        case NodeType::Number:   return "NUMBER";
        case NodeType::Add:      return "ADD";
        case NodeType::Subtract: return "SUBTRACT";
        case NodeType::Multiply: return "MULTIPLY";
        case NodeType::Divide:     return "DIVIDE";
        case NodeType::Result:     return "RESULT";
        case NodeType::Power:      return "POWER";
        case NodeType::Modulo:     return "MODULO";
        case NodeType::SquareRoot: return "SQUARE ROOT";
        case NodeType::Absolute:   return "ABSOLUTE";
        case NodeType::Sine:       return "SINE";
        case NodeType::Cosine:     return "COSINE";
        }

        return "NODE";
    }

    ImU32 NodeTitleColor(NodeType type)
    {
        switch (type)
        {
        case NodeType::Number:
            return IM_COL32(0, 145, 185, 255);

        case NodeType::Add:
            return IM_COL32(135, 48, 200, 255);

        case NodeType::Subtract:
            return IM_COL32(230, 90, 110, 255);

        case NodeType::Multiply:
            return IM_COL32(215, 120, 25, 255);

        case NodeType::Divide:
            return IM_COL32(45, 105, 210, 255);

        case NodeType::Result:
            return IM_COL32(25, 165, 105, 255);

        case NodeType::Power:
            return IM_COL32(170, 85, 220, 255);

        case NodeType::Modulo:
            return IM_COL32(70, 125, 225, 255);

        case NodeType::SquareRoot:
            return IM_COL32(20, 170, 150, 255);

        case NodeType::Absolute:
            return IM_COL32(90, 155, 80, 255);

        case NodeType::Sine:
            return IM_COL32(210, 90, 160, 255);

        case NodeType::Cosine:
            return IM_COL32(110, 95, 220, 255);
        }

        return IM_COL32(80, 90, 120, 255);
    }

    ImU32 BrightenColor(ImU32 baseColor)
    {
        const ImVec4 color =
            ImGui::ColorConvertU32ToFloat4(baseColor);

        return ImGui::ColorConvertFloat4ToU32(
            ImVec4(
                std::min(color.x + 0.12f, 1.0f),
                std::min(color.y + 0.12f, 1.0f),
                std::min(color.z + 0.12f, 1.0f),
                1.0f
            )
        );
    }

    void BeginStyledNode(const Node& node)
    {
        const ImU32 color = NodeTitleColor(node.type);
        const ImU32 brightColor = BrightenColor(color);

        ImNodes::PushColorStyle(ImNodesCol_TitleBar, color);
        ImNodes::PushColorStyle(
            ImNodesCol_TitleBarHovered,
            brightColor
        );
        ImNodes::PushColorStyle(
            ImNodesCol_TitleBarSelected,
            brightColor
        );

        ImNodes::BeginNode(node.id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(NodeTitle(node.type));
        ImNodes::EndNodeTitleBar();
    }

    void EndStyledNode()
    {
        ImNodes::EndNode();

        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
    }

    void BeginInputPin(int attributeId)
    {
        ImNodes::BeginInputAttribute(
            attributeId,
            ImNodesPinShape_CircleFilled
        );
    }

    void EndInputPin()
    {
        ImNodes::EndInputAttribute();
    }

    void BeginOutputPin(int attributeId)
    {
        ImNodes::BeginOutputAttribute(
            attributeId,
            ImNodesPinShape_CircleFilled
        );
    }

    void EndOutputPin()
    {
        ImNodes::EndOutputAttribute();
    }

    void DrawNumberNode(Node& node)
    {
        BeginStyledNode(node);

        ImGui::Spacing();

        ImNodes::BeginStaticAttribute(
            StaticAttributeId(node)
        );

        ImGui::TextDisabled("Deger");

        ImGui::PushID(node.id);
        ImGui::SetNextItemWidth(155.0f * g_zoom);

        // Step degerlerini 0 verdigimiz icin + / - dugmeleri yoktur.
        // Kutuyu tiklayip Ctrl+A yaparak degeri klavyeden yazabilirsin.
        if (
            ImGui::InputFloat(
                "##NumberValue",
                &node.value,
                0.0f,
                0.0f,
                "%.3f",
                ImGuiInputTextFlags_AutoSelectAll
            )
            )
        {
            g_unsavedChanges = true;
            g_statusMessage = "Number degeri degistirildi.";
        }

        ImGui::PopID();

        ImNodes::EndStaticAttribute();

        ImGui::Spacing();

        BeginOutputPin(OutputId(node));
        ImGui::Indent(95.0f * g_zoom);
        ImGui::TextUnformatted("Value");
        EndOutputPin();

        EndStyledNode();
    }

    void DrawMathNode(Node& node)
    {
        BeginStyledNode(node);

        ImGui::Spacing();

        BeginInputPin(InputAId(node));
        ImGui::TextUnformatted("A");
        EndInputPin();

        BeginInputPin(InputBId(node));
        ImGui::TextUnformatted("B");
        EndInputPin();

        ImGui::Spacing();

        ImNodes::BeginStaticAttribute(
            StaticAttributeId(node)
        );

        std::unordered_set<int> visitingNodes;

        const EvaluationResult evaluation =
            EvaluateOutput(OutputId(node), visitingNodes);

        if (evaluation.success)
        {
            ImGui::TextDisabled("Calculated output");
            ImGui::Text("%.3f", evaluation.value);
        }
        else
        {
            ImGui::TextDisabled(
                "%s",
                evaluation.message.c_str()
            );
        }

        ImNodes::EndStaticAttribute();

        ImGui::Spacing();

        BeginOutputPin(OutputId(node));
        ImGui::Indent(90.0f * g_zoom);

        if (evaluation.success)
        {
            ImGui::Text("%.2f", evaluation.value);
        }
        else
        {
            ImGui::TextUnformatted("--");
        }

        EndOutputPin();

        EndStyledNode();
    }

    void DrawUnaryNode(Node& node)
    {
        BeginStyledNode(node);

        ImGui::Spacing();

        BeginInputPin(InputAId(node));
        ImGui::TextUnformatted("Input");
        EndInputPin();

        ImGui::Spacing();

        ImNodes::BeginStaticAttribute(
            StaticAttributeId(node)
        );

        std::unordered_set<int> visitingNodes;

        const EvaluationResult evaluation =
            EvaluateOutput(
                OutputId(node),
                visitingNodes
            );

        if (evaluation.success)
        {
            ImGui::TextDisabled("Calculated output");
            ImGui::Text("%.3f", evaluation.value);
        }
        else
        {
            ImGui::TextDisabled(
                "%s",
                evaluation.message.c_str()
            );
        }

        ImNodes::EndStaticAttribute();

        ImGui::Spacing();

        BeginOutputPin(OutputId(node));
        ImGui::Indent(90.0f * g_zoom);

        if (evaluation.success)
        {
            ImGui::Text("%.2f", evaluation.value);
        }
        else
        {
            ImGui::TextUnformatted("--");
        }

        EndOutputPin();

        EndStyledNode();
    }

    void DrawResultNode(Node& node)
    {
        BeginStyledNode(node);

        ImGui::Spacing();

        BeginInputPin(InputAId(node));
        ImGui::TextUnformatted("Input");
        EndInputPin();

        ImGui::Spacing();

        ImNodes::BeginStaticAttribute(
            StaticAttributeId(node)
        );

        const EvaluationResult evaluation =
            EvaluateResultNode(node);

        ImGui::TextDisabled("Final result");
        ImGui::SetWindowFontScale(1.45f);

        if (evaluation.success)
        {
            ImGui::Text("%.3f", evaluation.value);
        }
        else
        {
            ImGui::TextUnformatted("--");
        }

        ImGui::SetWindowFontScale(1.0f);

        if (!evaluation.success)
        {
            ImGui::TextDisabled(
                "%s",
                evaluation.message.c_str()
            );
        }

        ImNodes::EndStaticAttribute();

        EndStyledNode();
    }

    void DrawNode(Node& node)
    {
        switch (node.type)
        {
        case NodeType::Number:
            DrawNumberNode(node);
            break;

        case NodeType::Add:
        case NodeType::Subtract:
        case NodeType::Multiply:
        case NodeType::Divide:
        case NodeType::Power:
        case NodeType::Modulo:
            DrawMathNode(node);
            break;

        case NodeType::SquareRoot:
        case NodeType::Absolute:
        case NodeType::Sine:
        case NodeType::Cosine:
            DrawUnaryNode(node);
            break;

        case NodeType::Result:
            DrawResultNode(node);
            break;
        }
    }

    EvaluationResult FindMainResult()
    {
        for (const Node& node : g_nodes)
        {
            if (node.type == NodeType::Result)
            {
                const EvaluationResult result =
                    EvaluateResultNode(node);

                if (result.success)
                {
                    return result;
                }
            }
        }

        return { false, 0.0f, "Tamamlanmis sonuc yok" };
    }
}

namespace NodeEditor
{
    void Initialize()
    {
        ImNodes::CreateContext();
        ImNodes::StyleColorsDark();

        ImNodesStyle& style = ImNodes::GetStyle();

        style.GridSpacing = 32.0f;
        style.NodeCornerRounding = 12.0f;
        style.NodePadding = ImVec2(14.0f, 12.0f);
        style.NodeBorderThickness = 1.5f;
        style.LinkThickness = 4.0f;
        style.LinkHoverDistance = 16.0f;
        style.PinCircleRadius = 8.0f;
        style.PinHoverRadius = 22.0f;

        style.Colors[ImNodesCol_GridBackground] =
            IM_COL32(7, 9, 17, 255);

        style.Colors[ImNodesCol_GridLine] =
            IM_COL32(31, 35, 50, 130);

        style.Colors[ImNodesCol_GridLinePrimary] =
            IM_COL32(50, 56, 76, 170);

        style.Colors[ImNodesCol_NodeBackground] =
            IM_COL32(23, 27, 42, 255);

        style.Colors[ImNodesCol_NodeBackgroundHovered] =
            IM_COL32(31, 37, 58, 255);

        style.Colors[ImNodesCol_NodeBackgroundSelected] =
            IM_COL32(39, 45, 70, 255);

        style.Colors[ImNodesCol_NodeOutline] =
            IM_COL32(83, 96, 145, 255);

        style.Colors[ImNodesCol_Pin] =
            IM_COL32(45, 215, 255, 255);

        style.Colors[ImNodesCol_PinHovered] =
            IM_COL32(180, 248, 255, 255);

        style.Colors[ImNodesCol_Link] =
            IM_COL32(195, 78, 255, 255);

        style.Colors[ImNodesCol_LinkHovered] =
            IM_COL32(235, 165, 255, 255);

        style.Colors[ImNodesCol_LinkSelected] =
            IM_COL32(255, 220, 255, 255);

        style.Colors[ImNodesCol_MiniMapBackground] =
            IM_COL32(17, 20, 31, 230);

        style.Colors[ImNodesCol_MiniMapOutline] =
            IM_COL32(90, 105, 150, 255);

        ApplyZoomStyle();
        LoadExampleGraph();
    }

    void Shutdown()
    {
        ClearGraph();
        ImNodes::DestroyContext();
    }

    void Draw()
    {
        const ImGuiViewport* viewport =
            ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        const ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin(
            "Visual Node Calculator",
            nullptr,
            windowFlags
        );

        const EvaluationResult mainResult =
            FindMainResult();

        // ====================================================
        // RESPONSIVE TOP PANEL
        // Menüyü tek uzun satır yerine düzenli satırlara böler.
        // Böylece küçük veya büyük pencerelerde tüm butonlar görünür.
        // ====================================================

        const float topPanelHeight = 166.0f;

        ImGui::BeginChild(
            "TopControlPanel",
            ImVec2(0.0f, topPanelHeight),
            false,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse
        );

        // ----------------------------------------------------
        // 1. SATIR: Başlık, sonuç ve kayıt durumu
        // ----------------------------------------------------

        ImGui::TextUnformatted("VISUAL NODE CALCULATOR");

        ImGui::SameLine();
        ImGui::TextDisabled(
            "   Node tabanli matematik calisma alani"
        );

        const char* saveState =
            g_unsavedChanges ? "[Unsaved]" : "[Saved]";

        char resultBuffer[128];

        if (mainResult.success)
        {
            std::snprintf(
                resultBuffer,
                sizeof(resultBuffer),
                "Result: %.3f   %s",
                mainResult.value,
                saveState
            );
        }
        else
        {
            std::snprintf(
                resultBuffer,
                sizeof(resultBuffer),
                "Result: --   %s",
                saveState
            );
        }

        const float resultWidth =
            ImGui::CalcTextSize(resultBuffer).x;

        const float rightEdge =
            ImGui::GetWindowContentRegionMax().x;

        if (
            ImGui::GetCursorPosX() +
            resultWidth <
            rightEdge
            )
        {
            ImGui::SameLine(
                rightEdge - resultWidth
            );
        }

        ImGui::TextDisabled("%s", resultBuffer);

        ImGui::Separator();

        // ----------------------------------------------------
        // 2. SATIR: Dosya ve graph işlemleri
        // ----------------------------------------------------

        if (ImGui::Button("New Graph"))
        {
            ClearGraph();
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Graph"))
        {
            SaveGraphToFile();
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Graph"))
        {
            LoadGraphFromFile();
        }

        ImGui::SameLine();

        if (ImGui::Button("Add Node"))
        {
            g_newNodePosition = ImGui::GetMousePos();
            ImGui::OpenPopup("CreateNodePopup");
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear All"))
        {
            ClearGraph();
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Example"))
        {
            LoadExampleGraph();
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Complex"))
        {
            LoadComplexGraph();
        }

        // ----------------------------------------------------
        // 3. SATIR: Düzenleme ve görünüm işlemleri
        // ----------------------------------------------------

        if (ImGui::Button("Copy"))
        {
            CopySelectedNodes();
        }

        ImGui::SameLine();

        if (ImGui::Button("Paste"))
        {
            PasteClipboardNodes();
        }

        ImGui::SameLine();

        if (ImGui::Button("Duplicate"))
        {
            DuplicateSelectedNodes();
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset View"))
        {
            ImNodes::EditorContextResetPanning(
                ImVec2(0.0f, 0.0f)
            );

            g_statusMessage = "Gorunum sifirlandi.";
        }

        ImGui::SameLine();

        if (ImGui::Button("Focus Result"))
        {
            if (g_lastResultNodeId >= 0)
            {
                ImNodes::EditorContextMoveToNode(
                    g_lastResultNodeId
                );

                g_statusMessage =
                    "Result node'una odaklanildi.";
            }
            else
            {
                g_statusMessage =
                    "Odaklanilacak Result node'u yok.";
            }
        }

        ImGui::SameLine();
        ImGui::Checkbox("MiniMap", &g_showMiniMap);

        ImGui::SameLine();

        if (ImGui::Button("Zoom -"))
        {
            SetGraphZoom(g_zoom - kZoomStep);
        }

        ImGui::SameLine();

        if (ImGui::Button("100%"))
        {
            ResetGraphZoom();
        }

        ImGui::SameLine();

        if (ImGui::Button("Zoom +"))
        {
            SetGraphZoom(g_zoom + kZoomStep);
        }

        ImGui::SameLine();

        ImGui::TextDisabled(
            "%d%%  (Ctrl + Mouse Wheel)",
            static_cast<int>(g_zoom * 100.0f + 0.5f)
        );

        // ----------------------------------------------------
        // 4. SATIR: Expression alanı
        // ----------------------------------------------------

        ImGui::TextUnformatted("Expression");
        ImGui::SameLine();

        const float createButtonWidth = 104.0f;
        const float exampleTextWidth = 270.0f;
        const float spacing =
            ImGui::GetStyle().ItemSpacing.x;

        float expressionWidth =
            ImGui::GetContentRegionAvail().x -
            createButtonWidth -
            exampleTextWidth -
            spacing * 3.0f;

        expressionWidth =
            std::max(expressionWidth, 220.0f);

        ImGui::SetNextItemWidth(expressionWidth);

        const bool enterPressed =
            ImGui::InputText(
                "##ExpressionCommand",
                g_expressionBuffer,
                sizeof(g_expressionBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        ImGui::SameLine();

        if (
            ImGui::Button("Create Graph") ||
            enterPressed
            )
        {
            BuildGraphFromExpression(
                g_expressionBuffer
            );
        }

        ImGui::SameLine();
        ImGui::TextDisabled(
            "Ornek: ((10 + 5) * 2 - 3) / 2"
        );

        ImGui::TextDisabled(
            "Durum: %s   |   Ctrl+Wheel Zoom   Ctrl+C Copy   Ctrl+V Paste   Ctrl+D Duplicate   Delete Sil",
            g_statusMessage.c_str()
        );

        ImGui::EndChild();

        ImGui::Separator();

        // Cursor'u kesin olarak sol kenara al.
        // Eski sürümde uzun SameLine zinciri editor alanını daraltabiliyordu.
        ImGui::SetCursorPosX(
            ImGui::GetStyle().WindowPadding.x
        );

        // ====================================================
        // NODE EDITOR - KALAN ALANIN TAMAMINI KULLANIR
        // ====================================================

        ImGui::SetWindowFontScale(g_zoom);

        ImNodes::BeginNodeEditor();

        for (Node& node : g_nodes)
        {
            DrawNode(node);
        }

        for (const Link& link : g_links)
        {
            ImNodes::Link(
                link.id,
                link.startAttribute,
                link.endAttribute
            );
        }

        if (g_showMiniMap)
        {
            ImNodes::MiniMap(
                0.18f,
                ImNodesMiniMapLocation_BottomRight
            );
        }

        ImNodes::EndNodeEditor();

        ImGui::SetWindowFontScale(1.0f);

        if (g_placeNewNode)
        {
            ImNodes::SetNodeScreenSpacePos(
                g_newNodeId,
                g_newNodePosition
            );

            g_placeNewNode = false;
        }

        if (
            ImNodes::IsEditorHovered() &&
            ImGui::GetIO().KeyCtrl &&
            std::fabs(ImGui::GetIO().MouseWheel) > 0.001f
            )
        {
            SetGraphZoom(
                g_zoom +
                ImGui::GetIO().MouseWheel * kZoomStep
            );
        }

        if (
            ImNodes::IsEditorHovered() &&
            ImGui::IsMouseDragging(
                ImGuiMouseButton_Left,
                2.0f
            ) &&
            !ImGui::GetIO().WantTextInput
            )
        {
            g_unsavedChanges = true;
        }

        int hoveredNodeId = -1;

        const bool nodeHovered =
            ImNodes::IsNodeHovered(
                &hoveredNodeId
            );

        const bool editorHovered =
            ImNodes::IsEditorHovered();

        if (
            editorHovered &&
            !nodeHovered &&
            ImGui::IsMouseReleased(
                ImGuiMouseButton_Right
            )
            )
        {
            g_newNodePosition =
                ImGui::GetMousePos();

            ImGui::OpenPopup(
                "CreateNodePopup"
            );
        }

        if (
            ImGui::BeginPopup(
                "CreateNodePopup"
            )
            )
        {
            ImGui::TextDisabled("CREATE NODE");
            ImGui::Separator();

            if (ImGui::MenuItem("Number"))
            {
                AddNode(
                    NodeType::Number,
                    g_newNodePosition,
                    0.0f
                );
            }

            ImGui::SeparatorText("MATH");

            if (ImGui::MenuItem("Add"))
            {
                AddNode(
                    NodeType::Add,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Subtract"))
            {
                AddNode(
                    NodeType::Subtract,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Multiply"))
            {
                AddNode(
                    NodeType::Multiply,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Divide"))
            {
                AddNode(
                    NodeType::Divide,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Power"))
            {
                AddNode(
                    NodeType::Power,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Modulo"))
            {
                AddNode(
                    NodeType::Modulo,
                    g_newNodePosition
                );
            }

            ImGui::SeparatorText("ADVANCED");

            if (ImGui::MenuItem("Square Root"))
            {
                AddNode(
                    NodeType::SquareRoot,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Absolute"))
            {
                AddNode(
                    NodeType::Absolute,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Sine"))
            {
                AddNode(
                    NodeType::Sine,
                    g_newNodePosition
                );
            }

            if (ImGui::MenuItem("Cosine"))
            {
                AddNode(
                    NodeType::Cosine,
                    g_newNodePosition
                );
            }

            ImGui::SeparatorText("OUTPUT");

            if (ImGui::MenuItem("Result"))
            {
                AddNode(
                    NodeType::Result,
                    g_newNodePosition
                );
            }

            ImGui::EndPopup();
        }

        int startAttribute = 0;
        int endAttribute = 0;

        if (
            ImNodes::IsLinkCreated(
                &startAttribute,
                &endAttribute
            )
            )
        {
            if (
                IsInputAttribute(startAttribute) &&
                IsOutputAttribute(endAttribute)
                )
            {
                std::swap(
                    startAttribute,
                    endAttribute
                );
            }

            if (
                IsConnectionAllowed(
                    startAttribute,
                    endAttribute
                )
                )
            {
                AddLink(
                    startAttribute,
                    endAttribute
                );
            }
        }

        int droppedFromAttribute = 0;

        if (
            ImNodes::IsLinkDropped(
                &droppedFromAttribute,
                false
            )
            )
        {
            g_statusMessage =
                "Baglanti tamamlanmadi: Cizgiyi mavi pinin tam ustune birak.";
        }

        int destroyedLinkId = 0;

        if (
            ImNodes::IsLinkDestroyed(
                &destroyedLinkId
            )
            )
        {
            DeleteLinkById(
                destroyedLinkId
            );
        }

        const ImGuiIO& io = ImGui::GetIO();

        if (!io.WantTextInput)
        {
            if (
                io.KeyCtrl &&
                ImGui::IsKeyPressed(
                    ImGuiKey_C,
                    false
                )
                )
            {
                CopySelectedNodes();
            }

            if (
                io.KeyCtrl &&
                ImGui::IsKeyPressed(
                    ImGuiKey_V,
                    false
                )
                )
            {
                PasteClipboardNodes();
            }

            if (
                io.KeyCtrl &&
                ImGui::IsKeyPressed(
                    ImGuiKey_D,
                    false
                )
                )
            {
                DuplicateSelectedNodes();
            }

            if (
                ImGui::IsKeyPressed(
                    ImGuiKey_Delete,
                    false
                )
                )
            {
                DeleteSelectedElements();
            }
        }

        ImGui::End();
    }
}