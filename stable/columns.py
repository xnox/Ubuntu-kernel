LEFT = 'left'
RIGHT = 'right'
CENTER = 'center'

def align(text, width=70, alignment=LEFT):
    ''' Align the "text" using the given alignment, padding to the given
    width. Strip off any existing whitespace on the side being aligned to
    and pad with spaces (' ') on the opposite side.

    Code from http://www.faqts.com/knowledge_base/view.phtml/aid/4476
    '''
    if alignment == CENTER:
        text = text.strip()
        space = width - len(text)
        return ' '*(space/2) + text + ' '*(space/2 + space%2)
    elif alignment == RIGHT:
        text = text.rstrip()
        space = width - len(text)
        return ' '*space + text
    else:
        text = text.lstrip()
        space = width - len(text)
        return text + ' '*space

class FormatTable:
    '''Format some data into a table with constraints on the widths of the
    columns and the alignment of the text inside the columns.
    '''
    def __init__(self, columninfo, rows, spacer=' | ', retain_newlines=True):
        '''
        "columninfo"   is a list of tuples (width in chars, alignment) where
                    alignment is one of LEFT, CENTER or RIGHT.
        "rows"  is an array of rows, each row is a list of column data.
        '''
        self.columninfo = columninfo
        self.num_columns = len(columninfo)
        self.rows = rows
        self.spacer = spacer
        self.retain_newlines = retain_newlines
        self.positions = [0]*self.num_columns

    def format(self):
        l = []
        for row in self.rows:
            assert len(self.columninfo) == len(row), \
                'columninfo and each row must be same length'
            rowout = self.spacer.lstrip()
            for col, (width, alignment) in enumerate(self.columninfo):
                cell = align(row[col], width, alignment);
                rowout= rowout + cell + self.spacer
            rowout.rstrip()
            l.append(rowout)
        return '\n'.join(l)
                
    def __str__(self):
        return self.format()
