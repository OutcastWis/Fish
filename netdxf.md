### CAD内嵌块不正确的问题
* @detail
> 文件A.dxf是由CAD生成的dxf文件, 里面存在嵌套块, 且块的单位不统一. 那么, 经过netdxf打开, 再保存成B.dxf. 使用CAD打开B.dxf会导致嵌套块的缩放不正确
* @reason
```c#
// in DxfWriter
private void WriteInsert(Insert insert)
{
    // ...

    // we need to apply the scaling factor between the block and the document, or the block that owns it in case of nested blocks
    double scale = UnitHelper.ConversionFactor(insert.Block.Record.Units, insert.Owner.Record.IsForInternalUseOnly ? this.doc.DrawingVariables.InsUnits : insert.Owner.Record.Units);

    this.chunk.Write(41, insert.Scale.X*scale);
    this.chunk.Write(42, insert.Scale.Y*scale);
    this.chunk.Write(43, insert.Scale.Z*scale);
    
    // ...
}

// in DxfReader
private void PostProcesses() {
    // ...

    // apply the units scale to the insertion scale (this is for not nested blocks)
    if (pair.Key is Insert insert)
    {
       double scale = UnitHelper.ConversionFactor(this.doc.DrawingVariables.InsUnits, insert.Block.Record.Units);
       insert.Scale *= scale;
    }
    // ... 
}
```
> netdxf在读取dxf时, 对于model_space下的insert, 其scale会由于PostProcesses()进行变化. 而内嵌的则不会. 同时, 在写dxf时, 所有insert.scale都会因为WriteInsert中的处理, 进行变化:
> 1. 令: K1=文档单位/自身块单位, K2=自身块单位/父亲块单位. 对第一层Insert而言, 父亲既是文档
> 2. 则第一层Insert.scale = Insert.scale * (K1, in reading) * (自身块单位/文档单位, 等价于K2, in writing), 
> 3. 内嵌Insert.scale = Insert.scale * (1, in reading) * (K2, in writing)
* @solution
```c#
/// <summary>
/// 调整内嵌块对应Insert的缩放比例. 抵消K2
/// </summary>
public static void rescale(BlockRecords bs, DrawingUnits doc_unit)
{
    foreach (var b in bs)
    {
        if (b.Owner.Owner == null) continue;

        var ss = b.Owner.Owner.GetReferences(b.Name);
        foreach (var r in ss)
        {
            if (r.Reference is not Insert i) continue;
            double scale = UnitHelper.ConversionFactor(i.Block.Record.Units, i.Owner.Record.Units);
            i.Scale /= scale;
        }
    }
}
/// <summary>
/// 调整第一层Insert(即父亲是*Model_Space)的缩放比例. 抵消K1
/// </summary>
public static void rescale_first(Insert i , DxfDocument doc) {
    double scale = UnitHelper.ConversionFactor(i.Block.Record.Units, doc.DrawingVariables.InsUnits);
    i.Scale *= scale;
}
/// <summary>
/// 复制块的单位
/// </summary>
public static void match_scale(BlockRecords target, BlockRecords source)
{
    foreach (var b in target)
    {    
        foreach (var o in source)
        {
            if (b.Name == o.Name)
                b.Record.Units = o.Record.Units;
        }
    }
}

public static void solution() {
    // 改演示函数约等于c++下, 
    // acdbHostApplicationServices()->workingDatabase()->insert(blockID, _T("wzj"), pDwg);
    // 其中pDwg指向A.dwg

    var input_doc = DxfDocument.Load("A.dxf");
    Block b = (Block)doc.Blocks[Block.DefaultModelSpaceName].Clone("wzj");
    out_doc.Blocks.Add(b);
    match_scale(out_doc.Blocks, doc.Blocks); // clone出来的块没有包含单位, 需要复制单位
    rescale(out_doc.Blocks, doc.DrawingVariables.InsUnits); // 调整内嵌块, 抵消K2
    foreach (var e in b.Entities)
    { // input_doc中的insert此时在b中, 进行调整, 抵消K1
        if (e is not Insert i) continue;
        rescale_first(i, input_doc);
    }
    out_doc.Save("B.dxf"); // 此时, B.dxf被CAD打开, 图形正常
}
```