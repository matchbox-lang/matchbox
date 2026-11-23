#include "compiler/parser/slice.h"
#include "compiler/ast/ast.h"
#include "compiler/parser/expression.h"

ASTNode* parse_slice(Parser* parser, ASTNode* index)
{
    ASTNode* slice = create_slice_node(index->index.token, index->index.array, index->index.index);
    
    index->index.array = NULL;
    index->index.index = NULL;

    free_ast_node(index);
    consume(parser, TokenRange);

    slice->slice.end = parse_expression(parser);

    if (!slice->slice.end || is_end_of_file(parser)) {
        free_ast_node(slice);

        return NULL;
    }

    consume(parser, TokenRsquare);

    return slice;
}
